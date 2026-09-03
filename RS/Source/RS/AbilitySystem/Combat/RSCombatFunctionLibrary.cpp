// Fill out your copyright notice in the Description page of Project Settings.

#include "RSCombatFunctionLibrary.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "RSGameplayTags.h"

namespace
{
	// Sphere는 수평 거리로 판정하므로 후보 수집도 높이에 좌우되지 않아야 합니다
	// 캡슐은 양 끝이 반구라 반지름이 그대로 유지되는 구간이 이 값의 위아래로만 보장됩니다
	constexpr float RSSphereQueryVerticalExtent = 300.0f;
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

#if ENABLE_DRAW_DEBUG
namespace
{
	void DrawRSCombatShape(const UWorld* World, const FRSCombatShape& Shape, const FTransform& ShapeTransform, const FColor& Color)
	{
		constexpr float DebugLifeTime = 1.0f;
		constexpr int32 DebugCircleSegments = 48;

		const FVector ShapeLocation = ShapeTransform.GetLocation();

		switch (Shape.Type)
		{
		case ERSCombatShapeType::Box:
			DrawDebugBox(World, ShapeLocation, Shape.BoxExtent, ShapeTransform.GetRotation(), Color, false, DebugLifeTime);
			break;

		case ERSCombatShapeType::Sphere:
			// 판정이 수평 거리 기준이므로 구가 아니라 바닥 평면의 원으로 그려야 실제 범위와 일치합니다
			DrawDebugCircle(World, ShapeLocation, Shape.Radius, DebugCircleSegments, Color, false, DebugLifeTime, 0, 0.0f, FVector::ForwardVector, FVector::RightVector, false);

			if (Shape.InnerRadius > 0.0f)
			{
				DrawDebugCircle(World, ShapeLocation, Shape.InnerRadius, DebugCircleSegments, Color, false, DebugLifeTime, 0, 0.0f, FVector::ForwardVector, FVector::RightVector, false);
			}
			break;
		}
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

	const bool bIsSphere = Shape.Type == ERSCombatShapeType::Sphere;
	const FVector ShapeLocation = ShapeTransform.GetLocation();

	// 축 정렬 박스를 사용하면 공격자가 대각선을 바라볼 때 판정이 어긋나므로 배치 회전을 함께 넘깁니다
	// Sphere는 회전이 의미가 없고 캡슐이 반드시 세로로 서 있어야 하므로 회전을 사용하지 않습니다
	const FQuat QueryRotation = bIsSphere ? FQuat::Identity : ShapeTransform.GetRotation();

	// 구를 쓰면 반지름이 작을 때 서 있는 높이 차이만으로 대상을 놓치므로 세로 캡슐로 후보를 모읍니다
	const FCollisionShape QueryShape = bIsSphere
		? FCollisionShape::MakeCapsule(Shape.Radius, Shape.Radius + RSSphereQueryVerticalExtent)
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

#if ENABLE_DRAW_DEBUG
	if (IsHitCheckDebugEnabled())
	{
		DrawRSCombatShape(World, Shape, ShapeTransform, OutTargets.IsEmpty() ? FColor::Silver : FColor::Red);
	}
#endif
}

bool URSCombatFunctionLibrary::IsHitCheckDebugEnabled()
{
#if !UE_BUILD_SHIPPING
	return bRSHitCheckDebugEnabled;
#else
	return false;
#endif
}
