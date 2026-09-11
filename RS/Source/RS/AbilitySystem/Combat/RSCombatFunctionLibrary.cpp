// Fill out your copyright notice in the Description page of Project Settings.

#include "RSCombatFunctionLibrary.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "RSAbilitySystemComponent.h"
#include "RSGameplayTags.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace
{
	// 형상과 무관하게 같은 층으로 취급할 높이입니다
	// 판정은 수평 거리만 보므로 이 값은 규칙이 아니라 후보를 놓치지 않을 만큼의 여유입니다
	constexpr float RSCombatQueryVerticalExtent = 300.0f;

	// 판정은 한 프레임만 실행되므로 결과를 눈으로 확인할 수 있을 만큼만 남깁니다
	constexpr float RSCombatDebugLifeTime = 1.0f;

	bool TryGetConeParameters(const FRSCombatShape& Shape, const FTransform& ShapeTransform, FVector& OutHorizontalForward, float& OutRangeSquared, float& OutMinimumDot)
	{
		if (Shape.Type != ERSCombatShapeType::Cone || !Shape.IsDataValid())
		{
			return false;
		}

		OutHorizontalForward = ShapeTransform.GetUnitAxis(EAxis::X);
		OutHorizontalForward.Z = 0.0f;
		if (!OutHorizontalForward.Normalize())
		{
			return false;
		}

		OutRangeSquared = FMath::Square(Shape.Range);
		OutMinimumDot = FMath::Cos(FMath::DegreesToRadians(Shape.Angle * 0.5f));

		return true;
	}

	bool IsLocationInsidePreparedCone(const FVector& ShapeLocation, const FVector& HorizontalForward, float RangeSquared, float MinimumDot, const FVector& TargetLocation)
	{
		FVector Offset = TargetLocation - ShapeLocation;
		Offset.Z = 0.0f;

		const float DistanceSquared = Offset.SizeSquared();
		if (DistanceSquared > RangeSquared && !FMath::IsNearlyEqual(DistanceSquared, RangeSquared))
		{
			return false;
		}

		// 꼭짓점에서는 방향이 정의되지 않지만 거리상 Cone 안이므로 각도 검사를 생략합니다
		if (FMath::IsNearlyZero(DistanceSquared))
		{
			return true;
		}

		const FVector DirectionToTarget = Offset.GetSafeNormal();
		const float ForwardDot = FVector::DotProduct(HorizontalForward, DirectionToTarget);

		return ForwardDot >= MinimumDot || FMath::IsNearlyEqual(ForwardDot, MinimumDot);
	}
}

bool FRSCombatShape::IsDataValid(FString* OutValidationError) const
{
	if (OutValidationError)
	{
		OutValidationError->Reset();
	}

	auto SetValidationError = [OutValidationError](const TCHAR* ErrorMessage)
		{
			if (OutValidationError)
			{
				*OutValidationError = ErrorMessage;
			}
		};

	switch (Type)
	{
	case ERSCombatShapeType::Box:
		if (BoxExtent.ContainsNaN() || BoxExtent.X <= 0.0f || BoxExtent.Y <= 0.0f || BoxExtent.Z <= 0.0f)
		{
			SetValidationError(TEXT("BoxExtent must contain finite values greater than zero."));

			return false;
		}
		break;

	case ERSCombatShapeType::Sphere:
		if (!FMath::IsFinite(Radius) || Radius <= 0.0f)
		{
			SetValidationError(TEXT("Radius must be finite and greater than zero."));

			return false;
		}

		if (!FMath::IsFinite(InnerRadius) || InnerRadius < 0.0f || InnerRadius >= Radius)
		{
			SetValidationError(TEXT("InnerRadius must be finite and satisfy 0 <= InnerRadius < Radius."));

			return false;
		}
		break;

	case ERSCombatShapeType::Cone:
		if (!FMath::IsFinite(Range) || Range <= 0.0f)
		{
			SetValidationError(TEXT("Range must be finite and greater than zero."));

			return false;
		}

		if (!FMath::IsFinite(Angle) || Angle <= 0.0f || Angle > 180.0f)
		{
			SetValidationError(TEXT("Angle must be finite and satisfy 0 < Angle <= 180 degrees."));

			return false;
		}
		break;

	default:
		SetValidationError(TEXT("Shape type is not supported."));

		return false;
	}

	return true;
}

#if !UE_BUILD_SHIPPING
namespace
{
	// 판정은 한 프레임만 실행되어 화면에 남는 것이 없으므로 확인용 출력을 콘솔에서 켜고 끕니다
	bool bRSHitCheckDebugEnabled = false;

	const TCHAR* RSToggleHitCheckDebugCommandName = TEXT("RS.Combat.ToggleHitCheckDebug");
}

namespace RSCombatDebug
{
	// FAutoConsoleCommand를 전역 객체로 두면 핫 리로드가 같은 이름을 다시 등록할 때 엔진이 이전 콘솔 객체를 삭제하는데
	// 이전 DLL에 남은 전역 객체는 삭제된 주소를 계속 들고 있다가 에디터 종료 시 소멸자에서 그 주소를 읽어 크래시가 납니다
	// 그래서 등록과 해제를 모듈 수명에 맡기고, 해제는 포인터가 아니라 이름으로 요청해 삭제된 주소를 건드리지 않습니다
	void RegisterConsoleCommands()
	{
		IConsoleManager::Get().RegisterConsoleCommand(RSToggleHitCheckDebugCommandName, TEXT("Toggles hit check debug shapes and hit result logs."),
			FConsoleCommandDelegate::CreateLambda([]()
				{
					bRSHitCheckDebugEnabled = !bRSHitCheckDebugEnabled;

					UE_LOG(LogTemp, Log, TEXT("Hit check debug %s"), bRSHitCheckDebugEnabled ? TEXT("enabled") : TEXT("disabled"));
				}));
	}

	void UnregisterConsoleCommands()
	{
		IConsoleManager::Get().UnregisterConsoleObject(RSToggleHitCheckDebugCommandName);
	}
}
#endif

void URSCombatFunctionLibrary::FindTargetsInShape(const AActor* Attacker, ECollisionChannel TargetChannel, const FRSCombatShape& Shape, const FTransform& ShapeTransform, TArray<AActor*>& OutTargets)
{
	OutTargets.Reset();

	if (!Attacker)
	{
		return;
	}

	const UWorld* World = Attacker->GetWorld();
	if (!World)
	{
		return;
	}

	const FVector ShapeLocation = ShapeTransform.GetLocation();
	const bool bIsSphere = Shape.Type == ERSCombatShapeType::Sphere;
	const bool bIsCone = Shape.Type == ERSCombatShapeType::Cone;

	FVector ConeHorizontalForward = FVector::ZeroVector;
	float ConeRangeSquared = 0.0f;
	float ConeMinimumDot = 0.0f;
	if (bIsCone && !TryGetConeParameters(Shape, ShapeTransform, ConeHorizontalForward, ConeRangeSquared, ConeMinimumDot))
	{
		return;
	}

	// 축 정렬 박스를 사용하면 공격자가 대각선을 바라볼 때 판정이 어긋나므로 배치 회전을 함께 넘깁니다
	// Sphere와 Cone은 실제 형상을 수평 필터에서 자르므로 후보 박스의 회전을 사용하지 않습니다
	const FQuat QueryRotation = bIsSphere || bIsCone ? FQuat::Identity : ShapeTransform.GetRotation();

	// 오버랩 형상은 판정 형상을 나타내지 않습니다. 판정 영역을 감싸기만 하면 되고 실제 판정은 아래 필터가 합니다
	// 그래서 형상마다 근사를 고르지 않고 항상 감싸는 박스로 모읍니다
	// 구나 캡슐은 높이에 따라 수평 도달 거리가 줄어들어 대상을 놓칠 수 있는데 박스는 어느 높이에서든 일정합니다
	FCollisionShape QueryShape;
	switch (Shape.Type)
	{
	case ERSCombatShapeType::Sphere:
		QueryShape = FCollisionShape::MakeBox(FVector(Shape.Radius, Shape.Radius, RSCombatQueryVerticalExtent));
		break;

	case ERSCombatShapeType::Cone:
		QueryShape = FCollisionShape::MakeBox(FVector(Shape.Range, Shape.Range, RSCombatQueryVerticalExtent));
		break;

	case ERSCombatShapeType::Box:
		QueryShape = FCollisionShape::MakeBox(Shape.BoxExtent);
		break;

	default:
		return;
	}

	FCollisionQueryParams QueryParams;
	QueryParams.bTraceComplex = false;
	QueryParams.AddIgnoredActor(Attacker);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByChannel(Overlaps, ShapeLocation, QueryRotation, TargetChannel, QueryShape, QueryParams);

	const float InnerRadiusSquared = FMath::Square(Shape.InnerRadius);
	const float RadiusSquared = FMath::Square(Shape.Radius);

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* OverlappedActor = Overlap.GetActor();

		// 캡슐과 Physics Asset이 같은 채널에 반응하면 한 액터가 여러 번 반환되어 한 타격의 피해가 중복 적용됩니다
		if (!IsValid(OverlappedActor) || OutTargets.Contains(OverlappedActor))
		{
			continue;
		}

		const UAbilitySystemComponent* TargetAbilitySystemComp = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OverlappedActor);
		if (!TargetAbilitySystemComp || TargetAbilitySystemComp->HasMatchingGameplayTag(RSGameplayTags::State_Dead))
		{
			continue;
		}

		if (bIsSphere)
		{
			// 후보 박스가 원보다 넓으므로 여기서 실제 형상으로 잘라냅니다
			// 경계는 액터 중심점으로 판정합니다. 표면 기준으로 바꾸면 안쪽과 바깥쪽 경계의 관대함이 달라집니다
			// InnerRadius가 0이면 안쪽 조건이 항상 참이 되어 구멍 없는 원이 됩니다
			const float DistanceSquared = FVector::DistSquared2D(ShapeLocation, OverlappedActor->GetActorLocation());
			if (DistanceSquared < InnerRadiusSquared || DistanceSquared >= RadiusSquared)
			{
				continue;
			}
		}
		else if (bIsCone && !IsLocationInsidePreparedCone(ShapeLocation, ConeHorizontalForward, ConeRangeSquared, ConeMinimumDot, OverlappedActor->GetActorLocation()))
		{
			continue;
		}

		OutTargets.Add(OverlappedActor);
	}

	if (IsHitCheckDebugEnabled())
	{
		DrawDebugCombatShape(World, Shape, ShapeTransform, OutTargets.IsEmpty() ? FColor::Silver : FColor::Red, RSCombatDebugLifeTime);
	}
}

bool URSCombatFunctionLibrary::IsLocationInsideCone(const FRSCombatShape& Shape, const FTransform& ShapeTransform, const FVector& TargetLocation)
{
	FVector HorizontalForward = FVector::ZeroVector;
	float RangeSquared = 0.0f;
	float MinimumDot = 0.0f;
	if (!TryGetConeParameters(Shape, ShapeTransform, HorizontalForward, RangeSquared, MinimumDot))
	{
		return false;
	}

	return IsLocationInsidePreparedCone(ShapeTransform.GetLocation(), HorizontalForward, RangeSquared, MinimumDot, TargetLocation);
}

void URSCombatFunctionLibrary::DrawDebugCombatShape(const UWorld* World, const FRSCombatShape& Shape, const FTransform& ShapeTransform, const FColor& Color, float LifeTime)
{
#if ENABLE_DRAW_DEBUG
	if (!World)
	{
		return;
	}

	constexpr int32 DebugCircleSegments = 48;

	const FVector ShapeLocation = ShapeTransform.GetLocation();

	switch (Shape.Type)
	{
	case ERSCombatShapeType::Box:
		DrawDebugBox(World, ShapeLocation, Shape.BoxExtent, ShapeTransform.GetRotation(), Color, false, LifeTime);
		break;

	case ERSCombatShapeType::Sphere:
		// 판정이 수평 거리 기준이므로 구가 아니라 바닥 평면의 원으로 그려야 실제 범위와 일치합니다
		DrawDebugCircle(World, ShapeLocation, Shape.Radius, DebugCircleSegments, Color, false, LifeTime, 0, 0.0f, FVector::ForwardVector, FVector::RightVector, false);

		if (Shape.InnerRadius > 0.0f)
		{
			DrawDebugCircle(World, ShapeLocation, Shape.InnerRadius, DebugCircleSegments, Color, false, LifeTime, 0, 0.0f, FVector::ForwardVector, FVector::RightVector, false);
		}
		break;

	case ERSCombatShapeType::Cone:
	{
		if (!Shape.IsDataValid())
		{
			break;
		}

		FVector HorizontalForward = ShapeTransform.GetUnitAxis(EAxis::X);
		HorizontalForward.Z = 0.0f;
		if (!HorizontalForward.Normalize())
		{
			break;
		}

		const FVector LeftBoundary = HorizontalForward.RotateAngleAxis(-Shape.Angle * 0.5f, FVector::UpVector);
		const FVector RightBoundary = HorizontalForward.RotateAngleAxis(Shape.Angle * 0.5f, FVector::UpVector);
		DrawDebugLine(World, ShapeLocation, ShapeLocation + LeftBoundary * Shape.Range, Color, false, LifeTime);
		DrawDebugLine(World, ShapeLocation, ShapeLocation + RightBoundary * Shape.Range, Color, false, LifeTime);

		FVector PreviousArcPoint = ShapeLocation + LeftBoundary * Shape.Range;
		for (int32 SegmentIndex = 1; SegmentIndex <= DebugCircleSegments; ++SegmentIndex)
		{
			const float SegmentRatio = static_cast<float>(SegmentIndex) / static_cast<float>(DebugCircleSegments);
			const float SegmentAngle = FMath::Lerp(-Shape.Angle * 0.5f, Shape.Angle * 0.5f, SegmentRatio);
			const FVector ArcPoint = ShapeLocation + HorizontalForward.RotateAngleAxis(SegmentAngle, FVector::UpVector) * Shape.Range;
			DrawDebugLine(World, PreviousArcPoint, ArcPoint, Color, false, LifeTime);
			PreviousArcPoint = ArcPoint;
		}
		break;
	}
	}
#endif
}

void URSCombatFunctionLibrary::SendHitReaction(const AActor* Instigator, AActor* TargetActor, const FRSHitReactionDefinition& ReactionDefinition)
{
	SendHitReactionInternal(Instigator, TargetActor, ReactionDefinition, false, FVector::ZeroVector);
}

bool URSCombatFunctionLibrary::ShouldPlayCameraShake(const FRSHitFeedbackDefinition& Feedback, bool bHasHitTargets)
{
	if (!Feedback.CameraShake)
	{
		return false;
	}

	return Feedback.CameraShakeTiming == ERSCameraShakeTiming::OnHitCheck || bHasHitTargets;
}

void URSCombatFunctionLibrary::PlayHitFeedback(AActor* Attacker, const TArray<AActor*>& HitTargets, const FRSHitFeedbackDefinition& Feedback)
{
	if (!Attacker)
	{
		return;
	}

	UWorld* World = Attacker->GetWorld();
	if (!World)
	{
		return;
	}

	const bool bHasHitTargets = !HitTargets.IsEmpty();

	// 셰이크는 보는 사람의 카메라를 흔드는 것이므로 공격자가 아니라 로컬 플레이어의 컨트롤러로 보냅니다
	// 보스는 AI 컨트롤러라 공격자를 따라가면 흔들 카메라가 없습니다
	if (ShouldPlayCameraShake(Feedback, bHasHitTargets))
	{
		if (APlayerController* ViewerController = UGameplayStatics::GetPlayerController(World, 0))
		{
			ViewerController->ClientStartCameraShake(Feedback.CameraShake, Feedback.CameraShakeScale);
		}
	}

	if (!bHasHitTargets)
	{
		return;
	}

	// 이펙트는 맞은 대상마다 나지만 소리는 한 타격에 하나여야 광역기에서 겹쳐 울리지 않습니다
	if (Feedback.ImpactNiagara)
	{
		for (const AActor* HitTarget : HitTargets)
		{
			if (HitTarget)
			{
				UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Feedback.ImpactNiagara, HitTarget->GetActorLocation());
			}
		}
	}

	if (Feedback.ImpactSound)
	{
		if (const AActor* FirstHitTarget = HitTargets[0])
		{
			UGameplayStatics::PlaySoundAtLocation(World, Feedback.ImpactSound, FirstHitTarget->GetActorLocation());
		}
	}

	// 히트스톱은 공격자의 시간만 늦추며, 값을 비워 둔 공격은 ApplyHitStop이 그대로 무시합니다
	if (URSAbilitySystemComponent* AttackerAbilitySystemComp = Cast<URSAbilitySystemComponent>(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Attacker)))
	{
		AttackerAbilitySystemComp->ApplyHitStop(Feedback.HitStopDuration, Feedback.HitStopTimeDilation);
	}
}

void URSCombatFunctionLibrary::SendHitReactionWithKnockbackDirection(const AActor* Instigator, AActor* TargetActor, const FRSHitReactionDefinition& ReactionDefinition, const FVector& KnockbackDirection)
{
	SendHitReactionInternal(Instigator, TargetActor, ReactionDefinition, true, KnockbackDirection);
}

void URSCombatFunctionLibrary::SendHitReactionInternal(const AActor* Instigator, AActor* TargetActor, const FRSHitReactionDefinition& ReactionDefinition, bool bHasKnockbackDirection, const FVector& KnockbackDirection)
{
	if (!TargetActor || ReactionDefinition.Type == ERSHitReactionType::None)
	{
		return;
	}

	FGameplayEventData ReactionPayload;
	ReactionPayload.Instigator = Instigator;
	ReactionPayload.Target = TargetActor;

	switch (ReactionDefinition.Type)
	{
	case ERSHitReactionType::HitReact:
		ReactionPayload.EventTag = RSGameplayTags::GameplayEvent_CrowdControl_HitReact;

		break;

	case ERSHitReactionType::Knockdown:
	{
		ReactionPayload.EventTag = RSGameplayTags::GameplayEvent_CrowdControl_Knockdown;

		// 넉다운은 거리와 높이, 시간이 타격마다 다르므로 float 하나인 EventMagnitude 대신 TargetData로 전달합니다
		FRSKnockbackTargetData* KnockbackData = new FRSKnockbackTargetData();
		KnockbackData->bHasKnockbackDirection = bHasKnockbackDirection;
		KnockbackData->KnockbackDirection = KnockbackDirection;
		KnockbackData->Distance = ReactionDefinition.KnockbackDistance;
		KnockbackData->Height = ReactionDefinition.KnockbackHeight;
		KnockbackData->Duration = ReactionDefinition.KnockbackDuration;
		ReactionPayload.TargetData.Add(KnockbackData);

		break;
	}

	default:
		return;
	}

	// 면역 판정은 대상의 반응 Ability가 하므로 여기서는 요청만 보냅니다
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(TargetActor, ReactionPayload.EventTag, ReactionPayload);
}

bool URSCombatFunctionLibrary::IsHitCheckDebugEnabled()
{
#if !UE_BUILD_SHIPPING
	return bRSHitCheckDebugEnabled;
#else
	return false;
#endif
}

#if WITH_EDITOR
bool URSCombatFunctionLibrary::ValidateIntegerDamage(const FScalableFloat& Damage, const FString& DamageLabel, FDataValidationContext& Context)
{
	// 커브 테이블을 참조하면 모든 레벨을 확인할 수 없으므로 기본 레벨의 평가값만 검사합니다
	const float DamageValue = Damage.GetValueAtLevel(1.0f);
	const float RoundedDamageValue = FMath::RoundToFloat(DamageValue);

	// 정확한 비교는 부동소수점 표현 오차에 걸리므로 의도적인 소수 입력만 걸러냅니다
	if (FMath::IsNearlyEqual(DamageValue, RoundedDamageValue, DamageIntegerTolerance))
	{
		return true;
	}

	Context.AddError(FText::FromString(FString::Printf(TEXT("%s의 피해량 %f는 정수가 아닙니다. RS의 데미지는 정수 단위입니다"), *DamageLabel, DamageValue)));

	return false;
}
#endif
