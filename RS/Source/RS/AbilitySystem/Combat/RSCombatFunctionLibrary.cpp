// Fill out your copyright notice in the Description page of Project Settings.

#include "RSCombatFunctionLibrary.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "RSGameplayTags.h"

namespace
{
	// 형상과 무관하게 같은 층으로 취급할 높이입니다
	// 판정은 수평 거리만 보므로 이 값은 규칙이 아니라 후보를 놓치지 않을 만큼의 여유입니다
	constexpr float RSCombatQueryVerticalExtent = 300.0f;

	// 판정은 한 프레임만 실행되므로 결과를 눈으로 확인할 수 있을 만큼만 남깁니다
	constexpr float RSCombatDebugLifeTime = 1.0f;
}

#if !UE_BUILD_SHIPPING
namespace
{
	// 판정은 한 프레임만 실행되어 화면에 남는 것이 없으므로 확인용 출력을 콘솔에서 켜고 끕니다
	bool bRSHitCheckDebugEnabled = false;

	FAutoConsoleCommand RSToggleHitCheckDebugCommand(TEXT("RS.Combat.ToggleHitCheckDebug"), TEXT("Toggles hit check debug shapes and hit result logs."),
		FConsoleCommandDelegate::CreateLambda([]()
			{
				bRSHitCheckDebugEnabled = !bRSHitCheckDebugEnabled;

				UE_LOG(LogTemp, Log, TEXT("Hit check debug %s"), bRSHitCheckDebugEnabled ? TEXT("enabled") : TEXT("disabled"));
			}));
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

	const bool bIsSphere = Shape.Type == ERSCombatShapeType::Sphere;
	const FVector ShapeLocation = ShapeTransform.GetLocation();

	// 축 정렬 박스를 사용하면 공격자가 대각선을 바라볼 때 판정이 어긋나므로 배치 회전을 함께 넘깁니다
	// Sphere는 회전이 의미가 없고 후보 박스가 세로로 서 있어야 하므로 회전을 사용하지 않습니다
	const FQuat QueryRotation = bIsSphere ? FQuat::Identity : ShapeTransform.GetRotation();

	// 오버랩 형상은 판정 형상을 나타내지 않습니다. 판정 영역을 감싸기만 하면 되고 실제 판정은 아래 필터가 합니다
	// 그래서 형상마다 근사를 고르지 않고 항상 감싸는 박스로 모읍니다
	// 구나 캡슐은 높이에 따라 수평 도달 거리가 줄어들어 대상을 놓칠 수 있는데 박스는 어느 높이에서든 일정합니다
	const FCollisionShape QueryShape = bIsSphere
		? FCollisionShape::MakeBox(FVector(Shape.Radius, Shape.Radius, RSCombatQueryVerticalExtent))
		: FCollisionShape::MakeBox(Shape.BoxExtent);

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

		OutTargets.Add(OverlappedActor);
	}

	if (IsHitCheckDebugEnabled())
	{
		DrawDebugCombatShape(World, Shape, ShapeTransform, OutTargets.IsEmpty() ? FColor::Silver : FColor::Red, RSCombatDebugLifeTime);
	}
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
	}
#endif
}

void URSCombatFunctionLibrary::SendHitReaction(const AActor* Instigator, AActor* TargetActor, const FRSHitReactionDefinition& ReactionDefinition)
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
