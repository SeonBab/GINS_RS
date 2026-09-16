// Fill out your copyright notice in the Description page of Project Settings.

#include "RSCombatFunctionLibrary.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SkeletalMeshComponent.h"
#include "NiagaraComponent.h"
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

	// 간격이 좁을수록 순회할 칸이 제곱으로 늘어나므로 채우기 전에 막을 상한입니다
	// 기획이 쓰는 밀도로는 닿지 않는 값이며, 설정 실수로 프레임이 멈추는 것만 방지합니다
	constexpr int64 RSShapeFillMaxCandidateCells = 4096;

	bool BuildAnnularSectorFill(const FRSCombatShape& Shape, const FTransform& ShapeTransform, float Spacing, TArray<FTransform>& OutTransforms)
	{
		const FRSAnnularSectorBounds Bounds = Shape.GetAnnularSectorBounds();

		// 격자 축을 형상 회전에 맞춰야 같은 형상이라도 배치가 회전을 따라갑니다
		FVector HorizontalForward = ShapeTransform.GetUnitAxis(EAxis::X);
		HorizontalForward.Z = 0.0f;
		if (!HorizontalForward.Normalize())
		{
			return false;
		}

		const FVector HorizontalRight = FVector::CrossProduct(FVector::UpVector, HorizontalForward);
		const int32 StepCount = FMath::FloorToInt(Bounds.OuterRadius / Spacing);

		// 포함 검사보다 먼저 막아야 간격이 좁을 때 순회 자체가 폭주하지 않습니다
		const int64 AxisCellCount = static_cast<int64>(StepCount) * 2 + 1;
		if (AxisCellCount * AxisCellCount > RSShapeFillMaxCandidateCells)
		{
			return false;
		}

		const FVector SectorCenter = ShapeTransform.GetLocation();
		const FQuat FillRotation = ShapeTransform.GetRotation();

		OutTransforms.Reset();
		for (int32 ForwardStep = -StepCount; ForwardStep <= StepCount; ++ForwardStep)
		{
			for (int32 LateralStep = -StepCount; LateralStep <= StepCount; ++LateralStep)
			{
				// 판정과 같은 커널로 걸러야 연출이 판정 범위를 벗어나거나 빈 가운데를 채우지 않습니다
				const FVector CellLocation = SectorCenter + HorizontalForward * (ForwardStep * Spacing) + HorizontalRight * (LateralStep * Spacing);
				if (!URSCombatFunctionLibrary::IsLocationInsideAnnularSector(ShapeTransform, Bounds, CellLocation))
				{
					continue;
				}

				OutTransforms.Emplace(FillRotation, CellLocation);
			}
		}

		// 간격이 형상보다 넓으면 격자가 한 칸으로 무너집니다
		// 그 한 칸은 중심이나 꼭짓점이라 공격 범위를 나타내지 못하므로, 두 반지름과 각도의 가운데에 하나만 둡니다
		if (OutTransforms.Num() <= 1)
		{
			OutTransforms.Reset();

			const float RepresentativeRadius = (Bounds.InnerRadius + Bounds.OuterRadius) * 0.5f;
			const float RepresentativeYawOffset = Bounds.CoversEveryAngle() ? 0.0f : Bounds.StartYawOffset + Bounds.SweepAngleDegrees * 0.5f;
			const FVector RepresentativeDirection = HorizontalForward.RotateAngleAxis(RepresentativeYawOffset, FVector::UpVector);

			OutTransforms.Emplace(FillRotation, SectorCenter + RepresentativeDirection * RepresentativeRadius);
		}

		return true;
	}
}

bool FRSAnnularSectorBounds::IsDataValid(FString* OutValidationError) const
{
	if (OutValidationError)
	{
		OutValidationError->Reset();
	}

	const auto SetValidationError = [OutValidationError](const TCHAR* ErrorMessage)
		{
			if (OutValidationError)
			{
				*OutValidationError = ErrorMessage;
			}
		};

	if (!FMath::IsFinite(OuterRadius) || !FMath::IsFinite(InnerRadius) || InnerRadius < 0.0f || OuterRadius <= InnerRadius)
	{
		SetValidationError(TEXT("OuterRadius must be finite and satisfy 0 <= InnerRadius < OuterRadius."));

		return false;
	}

	if (!FMath::IsFinite(StartYawOffset))
	{
		SetValidationError(TEXT("StartYawOffset must be finite."));

		return false;
	}

	// 각도는 래핑되므로 시작 각도에는 범위 제한이 필요하지 않습니다
	if (!FMath::IsFinite(SweepAngleDegrees) || FMath::IsNearlyZero(SweepAngleDegrees) || FMath::Abs(SweepAngleDegrees) > 360.0f)
	{
		SetValidationError(TEXT("SweepAngleDegrees must be finite, non-zero and within a full turn."));

		return false;
	}

	return true;
}

bool FRSAnnularSectorBounds::CoversEveryAngle() const
{
	return FMath::IsNearlyEqual(FMath::Abs(SweepAngleDegrees), 360.0f);
}

FRSAnnularSectorBounds FRSCombatShape::GetAnnularSectorBounds() const
{
	FRSAnnularSectorBounds Bounds;
	Bounds.InnerRadius = InnerRadius;
	Bounds.OuterRadius = OuterRadius;
	Bounds.StartYawOffset = StartYawOffset;
	Bounds.SweepAngleDegrees = SweepAngleDegrees;

	return Bounds;
}

bool FRSCombatShape::IsDataValid(FString* OutValidationError) const
{
	if (OutValidationError)
	{
		OutValidationError->Reset();
	}

	const auto SetValidationError = [OutValidationError](const TCHAR* ErrorMessage)
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

	// 경계 규칙은 판정 커널과 하나를 공유해야 검증을 통과한 형상이 런타임에 거부되지 않습니다
	case ERSCombatShapeType::AnnularSector:
		return GetAnnularSectorBounds().IsDataValid(OutValidationError);

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
	FindTargetsInShapeInternal(Attacker, TargetChannel, Shape, ShapeTransform, OutTargets, true);
}

void URSCombatFunctionLibrary::FindTargetsInShapeWithoutDebugDraw(const AActor* Attacker, ECollisionChannel TargetChannel, const FRSCombatShape& Shape, const FTransform& ShapeTransform, TArray<AActor*>& OutTargets)
{
	FindTargetsInShapeInternal(Attacker, TargetChannel, Shape, ShapeTransform, OutTargets, false);
}

void URSCombatFunctionLibrary::FindTargetsInShapeInternal(const AActor* Attacker, ECollisionChannel TargetChannel, const FRSCombatShape& Shape, const FTransform& ShapeTransform, TArray<AActor*>& OutTargets, bool bShouldDrawDebug)
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
	const bool bIsAnnularSector = Shape.Type == ERSCombatShapeType::AnnularSector;
	const FRSAnnularSectorBounds SectorBounds = Shape.GetAnnularSectorBounds();

	if (bIsAnnularSector && !SectorBounds.IsDataValid())
	{
		return;
	}

	// 축 정렬 박스를 사용하면 공격자가 대각선을 바라볼 때 판정이 어긋나므로 배치 회전을 함께 넘깁니다
	// 환형 부채꼴은 실제 형상을 수평 필터에서 자르므로 후보 박스의 회전을 사용하지 않습니다
	const FQuat QueryRotation = bIsAnnularSector ? FQuat::Identity : ShapeTransform.GetRotation();

	// 오버랩 형상은 판정 형상을 나타내지 않습니다. 판정 영역을 감싸기만 하면 되고 실제 판정은 아래 필터가 합니다
	// 그래서 형상마다 근사를 고르지 않고 항상 감싸는 박스로 모읍니다
	// 구나 캡슐은 높이에 따라 수평 도달 거리가 줄어들어 대상을 놓칠 수 있는데 박스는 어느 높이에서든 일정합니다
	FCollisionShape QueryShape;
	switch (Shape.Type)
	{
	case ERSCombatShapeType::AnnularSector:
		QueryShape = FCollisionShape::MakeBox(FVector(Shape.OuterRadius, Shape.OuterRadius, RSCombatQueryVerticalExtent));
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

		// 후보 박스가 형상보다 넓으므로 여기서 실제 형상으로 잘라냅니다
		// 경계는 액터 중심점으로 판정합니다. 표면 기준으로 바꾸면 안쪽과 바깥쪽 경계의 관대함이 달라집니다
		if (bIsAnnularSector && !IsLocationInsideAnnularSector(ShapeTransform, SectorBounds, OverlappedActor->GetActorLocation()))
		{
			continue;
		}

		OutTargets.Add(OverlappedActor);
	}

	if (bShouldDrawDebug && IsHitCheckDebugEnabled())
	{
		DrawDebugCombatShape(World, Shape, ShapeTransform, OutTargets.IsEmpty() ? FColor::Silver : FColor::Red, RSCombatDebugLifeTime);
	}
}

bool URSCombatFunctionLibrary::TryGetActorGroundLocation(const AActor* Actor, FVector& OutGroundLocation)
{
	if (!Actor)
	{
		return false;
	}

	const ACharacter* Character = Cast<const ACharacter>(Actor);
	const UCapsuleComponent* CapsuleComp = Character ? Character->GetCapsuleComponent() : nullptr;

	// 캡슐을 가진 액터는 자기 Up을 따라 내려야 눕거나 기울어진 순간에도 바닥이 캡슐과 함께 움직입니다
	// 캡슐이 없는 연출 액터까지 같은 계약으로 받아야 호출처가 액터 타입을 나누지 않습니다
	const FVector GroundLocation = CapsuleComp
		? CapsuleComp->GetComponentLocation() - CapsuleComp->GetUpVector() * CapsuleComp->GetScaledCapsuleHalfHeight()
		: Actor->GetActorLocation() - FVector::UpVector * Actor->GetSimpleCollisionHalfHeight();

	// 잘못된 위치에 연출을 만드는 대신 호출자가 그 실행을 포기할 수 있도록 실패를 알립니다
	if (GroundLocation.ContainsNaN())
	{
		return false;
	}

	OutGroundLocation = GroundLocation;

	return true;
}

bool URSCombatFunctionLibrary::IsLocationInsideAnnularSector(const FTransform& SectorTransform, const FRSAnnularSectorBounds& SectorBounds, const FVector& TargetLocation)
{
	if (SectorTransform.ContainsNaN() || TargetLocation.ContainsNaN() || !SectorBounds.IsDataValid())
	{
		return false;
	}

	const FVector LocalOffset = SectorTransform.InverseTransformPositionNoScale(TargetLocation);
	const float RadiusSquared = FMath::Square(LocalOffset.X) + FMath::Square(LocalOffset.Y);

	// 반지름은 반개구간이라 경계는 바깥쪽 형상의 것입니다
	// 이어 붙는 링이 경계를 공유해도 한 대상이 두 링에 걸리지 않으므로 호출자가 중복을 지울 필요가 없습니다
	if (RadiusSquared < FMath::Square(SectorBounds.InnerRadius) || RadiusSquared >= FMath::Square(SectorBounds.OuterRadius))
	{
		return false;
	}

	// 중심에서는 방향이 정의되지 않으므로 반지름 검사를 통과한 것으로 충분합니다
	// 360도는 시작 경계와 끝 경계가 같은 지점이라 반개구간을 그대로 적용하면 그 한 방향만 빠집니다
	if (RadiusSquared <= KINDA_SMALL_NUMBER || SectorBounds.CoversEveryAngle())
	{
		return true;
	}

	const float TargetYawOffset = FMath::RadiansToDegrees(FMath::Atan2(LocalOffset.Y, LocalOffset.X));
	const float DirectedAngle = SectorBounds.SweepAngleDegrees > 0.0f
		? FRotator::ClampAxis(TargetYawOffset - SectorBounds.StartYawOffset)
		: FRotator::ClampAxis(SectorBounds.StartYawOffset - TargetYawOffset);

	// 각도도 같은 규약입니다. ClampAxis가 [0, 360)을 돌려주므로 시작 경계는 0이 되어 포함됩니다
	return DirectedAngle < FMath::Abs(SectorBounds.SweepAngleDegrees);
}

bool URSCombatFunctionLibrary::TryGetActorHorizontalRadius(const AActor* Actor, float& OutHorizontalRadius)
{
	if (!Actor)
	{
		return false;
	}

	const ACharacter* Character = Cast<const ACharacter>(Actor);
	const UCapsuleComponent* CapsuleComp = Character ? Character->GetCapsuleComponent() : nullptr;

	// 크기를 키운 보스의 시작 반경이 함께 커져야 하므로 Scale을 반영한 반지름을 읽습니다
	// 캡슐이 없는 액터까지 같은 계약으로 받아야 호출처가 액터 타입을 나누지 않습니다
	const float HorizontalRadius = CapsuleComp
		? CapsuleComp->GetScaledCapsuleRadius()
		: Actor->GetSimpleCollisionRadius();

	// 잘못된 반경으로 판정 범위를 자르는 대신 호출자가 다른 선택을 할 수 있도록 실패를 알립니다
	if (!FMath::IsFinite(HorizontalRadius) || HorizontalRadius < 0.0f)
	{
		return false;
	}

	OutHorizontalRadius = HorizontalRadius;

	return true;
}

bool URSCombatFunctionLibrary::TryApplyMinimumInnerRadius(FRSAnnularSectorBounds& InOutBounds, float MinimumInnerRadius)
{
	if (!InOutBounds.IsDataValid())
	{
		return false;
	}

	// 하한이 없으면 경계를 그대로 둡니다. 하한은 바닥이지 덮어쓰기가 아닙니다
	if (!FMath::IsFinite(MinimumInnerRadius) || MinimumInnerRadius <= 0.0f)
	{
		return true;
	}

	// 경계 전체가 기준 액터 안에 있으면 판정할 면적이 남지 않으므로 호출자가 그 형상을 건너뛸 수 있게 알립니다
	if (MinimumInnerRadius >= InOutBounds.OuterRadius)
	{
		return false;
	}

	InOutBounds.InnerRadius = FMath::Max(InOutBounds.InnerRadius, MinimumInnerRadius);

	return true;
}

bool URSCombatFunctionLibrary::TryApplyMinimumInnerRadius(FRSCombatShape& InOutShape, float MinimumInnerRadius)
{
	if (!InOutShape.IsDataValid())
	{
		return false;
	}

	// Box를 보스 원점 기준으로 쓰는 패턴이 없어 자를 대상이 없습니다
	if (InOutShape.Type != ERSCombatShapeType::AnnularSector)
	{
		return true;
	}

	// 하한 규칙은 경계 층이 소유하므로 형상은 경계로 바꿔 넘기고 결과만 되돌려 받습니다
	FRSAnnularSectorBounds Bounds = InOutShape.GetAnnularSectorBounds();
	if (!TryApplyMinimumInnerRadius(Bounds, MinimumInnerRadius))
	{
		return false;
	}

	InOutShape.InnerRadius = Bounds.InnerRadius;

	return true;
}

bool URSCombatFunctionLibrary::BuildShapeFillTransforms(const FRSCombatShape& Shape, const FTransform& ShapeTransform, float Spacing, TArray<FTransform>& OutTransforms)
{
	if (!FMath::IsFinite(Spacing) || Spacing <= 0.0f || !Shape.IsDataValid())
	{
		return false;
	}

	switch (Shape.Type)
	{
	case ERSCombatShapeType::AnnularSector:
		return BuildAnnularSectorFill(Shape, ShapeTransform, Spacing, OutTransforms);

	default:
		// Box를 채우는 보스 패턴이 없어 지원하지 않으며, 조용히 비는 대신 호출자가 알 수 있게 실패합니다
		return false;
	}
}

void URSCombatFunctionLibrary::DrawDebugCombatShape(const UWorld* World, const FRSCombatShape& Shape, const FTransform& ShapeTransform, const FColor& Color, float LifeTime)
{
#if ENABLE_DRAW_DEBUG
	if (!World)
	{
		return;
	}

	switch (Shape.Type)
	{
	case ERSCombatShapeType::Box:
		DrawDebugBox(World, ShapeTransform.GetLocation(), Shape.BoxExtent, ShapeTransform.GetRotation(), Color, false, LifeTime);
		break;

	case ERSCombatShapeType::AnnularSector:
	{
		// 판정이 수평 거리와 각도 기준이므로 바닥 평면의 환형 부채꼴로 그려야 실제 범위와 일치합니다
		const FRSAnnularSectorBounds Bounds = Shape.GetAnnularSectorBounds();
		DrawDebugCombatAnnularSector(World, ShapeTransform, Bounds.InnerRadius, Bounds.OuterRadius, Bounds.StartYawOffset, Bounds.SweepAngleDegrees, Color, LifeTime);
		break;
	}
	}
#endif
}


void URSCombatFunctionLibrary::DrawDebugCombatAnnularSector(const UWorld* World, const FTransform& ShapeTransform, float InnerRadius, float OuterRadius, float StartAngleOffsetDegrees, float SweepAngleDegrees, const FColor& Color, float LifeTime)
{
#if ENABLE_DRAW_DEBUG
	const float AbsoluteSweepAngleDegrees = FMath::Abs(SweepAngleDegrees);
	if (!World || ShapeTransform.ContainsNaN() || !FMath::IsFinite(InnerRadius) || !FMath::IsFinite(OuterRadius)
		|| InnerRadius < 0.0f || OuterRadius <= InnerRadius || !FMath::IsFinite(StartAngleOffsetDegrees)
		|| !FMath::IsFinite(SweepAngleDegrees) || AbsoluteSweepAngleDegrees <= 0.0f || AbsoluteSweepAngleDegrees > 360.0f)
	{
		return;
	}

	FVector HorizontalForward = ShapeTransform.GetUnitAxis(EAxis::X);
	HorizontalForward.Z = 0.0f;
	if (!HorizontalForward.Normalize())
	{
		return;
	}

	constexpr int32 FullCircleSegmentCount = 48;
	const int32 SegmentCount = FMath::Max(1, FMath::CeilToInt(AbsoluteSweepAngleDegrees / 360.0f * FullCircleSegmentCount));
	const FVector ShapeLocation = ShapeTransform.GetLocation();
	const FVector StartBoundary = HorizontalForward.RotateAngleAxis(StartAngleOffsetDegrees, FVector::UpVector);
	if (!FMath::IsNearlyEqual(AbsoluteSweepAngleDegrees, 360.0f))
	{
		const FVector EndBoundary = StartBoundary.RotateAngleAxis(SweepAngleDegrees, FVector::UpVector);
		DrawDebugLine(World, ShapeLocation + StartBoundary * InnerRadius, ShapeLocation + StartBoundary * OuterRadius, Color, false, LifeTime);
		DrawDebugLine(World, ShapeLocation + EndBoundary * InnerRadius, ShapeLocation + EndBoundary * OuterRadius, Color, false, LifeTime);
	}

	const bool bHasInnerArc = InnerRadius > KINDA_SMALL_NUMBER;
	FVector PreviousInnerArcPoint = ShapeLocation + StartBoundary * InnerRadius;
	FVector PreviousOuterArcPoint = ShapeLocation + StartBoundary * OuterRadius;
	for (int32 SegmentIndex = 1; SegmentIndex <= SegmentCount; ++SegmentIndex)
	{
		const float SegmentRatio = static_cast<float>(SegmentIndex) / static_cast<float>(SegmentCount);
		const FVector ArcDirection = StartBoundary.RotateAngleAxis(SweepAngleDegrees * SegmentRatio, FVector::UpVector);
		const FVector InnerArcPoint = ShapeLocation + ArcDirection * InnerRadius;
		const FVector OuterArcPoint = ShapeLocation + ArcDirection * OuterRadius;
		if (bHasInnerArc)
		{
			DrawDebugLine(World, PreviousInnerArcPoint, InnerArcPoint, Color, false, LifeTime);
		}
		DrawDebugLine(World, PreviousOuterArcPoint, OuterArcPoint, Color, false, LifeTime);
		PreviousInnerArcPoint = InnerArcPoint;
		PreviousOuterArcPoint = OuterArcPoint;
	}
#endif
}

void URSCombatFunctionLibrary::SendHitReaction(const AActor* Instigator, AActor* TargetActor, const FRSHitReactionDefinition& ReactionDefinition)
{
	SendHitReactionInternal(Instigator, TargetActor, ReactionDefinition, false, FVector::ZeroVector);
}

void URSCombatFunctionLibrary::PlayCameraShake(const UObject* WorldContextObject, const FRSCameraShakeDefinition& ShakeDefinition)
{
	if (!ShakeDefinition.ShakeClass)
	{
		return;
	}

	// 셰이크는 보는 사람의 카메라를 흔드는 것이므로 요청자가 아니라 로컬 플레이어의 컨트롤러로 보냅니다
	// 보스는 AI 컨트롤러라 요청자를 따라가면 흔들 카메라가 없습니다
	if (APlayerController* ViewerController = UGameplayStatics::GetPlayerController(WorldContextObject, 0))
	{
		ViewerController->ClientStartCameraShake(ShakeDefinition.ShakeClass, ShakeDefinition.Scale);
	}
}

UNiagaraComponent* URSCombatFunctionLibrary::SpawnNiagaraFromDefinition(const UObject* WorldContextObject, USkeletalMeshComponent* MeshComponent, const FRSNiagaraSpawnDefinition& Definition, const FTransform& WorldTransform, bool bAutoDestroy)
{
	if (!Definition.NiagaraSystem)
	{
		return nullptr;
	}

	if (Definition.SpawnMode == ERSNiagaraSpawnMode::WorldTransform)
	{
		return UNiagaraFunctionLibrary::SpawnSystemAtLocation(WorldContextObject, Definition.NiagaraSystem, WorldTransform.GetLocation(), WorldTransform.Rotator(), WorldTransform.GetScale3D(), bAutoDestroy, true);
	}

	if (!MeshComponent || !MeshComponent->DoesSocketExist(Definition.SocketName))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s skipped Niagara %s because socket %s is unavailable"), *GetNameSafe(WorldContextObject), *GetNameSafe(Definition.NiagaraSystem), *Definition.SocketName.ToString());

		return nullptr;
	}

	if (Definition.SpawnMode == ERSNiagaraSpawnMode::SocketSnapshot)
	{
		const FTransform SocketTransform = MeshComponent->GetSocketTransform(Definition.SocketName);
		const FRotator SpawnRotation = Definition.bUseSocketRotation ? SocketTransform.Rotator() : FRotator::ZeroRotator;
		const FVector SpawnScale = Definition.bUseSocketScale ? SocketTransform.GetScale3D() : FVector::OneVector;

		return UNiagaraFunctionLibrary::SpawnSystemAtLocation(WorldContextObject, Definition.NiagaraSystem, SocketTransform.GetLocation(), SpawnRotation, SpawnScale, bAutoDestroy, true);
	}

	UNiagaraComponent* AttachedComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(Definition.NiagaraSystem, MeshComponent, Definition.SocketName, FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::SnapToTarget, bAutoDestroy, true);
	if (!AttachedComponent)
	{
		return nullptr;
	}

	// 절대 모드에서는 상대 값이 월드 값으로 해석되므로 소켓 위치만 따라가고 회전과 스케일은 월드 기준으로 고정됩니다
	if (!Definition.bUseSocketRotation)
	{
		AttachedComponent->SetUsingAbsoluteRotation(true);
		AttachedComponent->SetRelativeRotation(FRotator::ZeroRotator);
	}

	if (!Definition.bUseSocketScale)
	{
		AttachedComponent->SetUsingAbsoluteScale(true);
		AttachedComponent->SetRelativeScale3D(FVector::OneVector);
	}

	return AttachedComponent;
}

bool URSCombatFunctionLibrary::ShouldPlayCameraShake(const FRSHitFeedbackDefinition& Feedback, bool bHasHitTargets)
{
	if (!Feedback.CameraShake.ShakeClass)
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

	if (ShouldPlayCameraShake(Feedback, bHasHitTargets))
	{
		PlayCameraShake(Attacker, Feedback.CameraShake);
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
