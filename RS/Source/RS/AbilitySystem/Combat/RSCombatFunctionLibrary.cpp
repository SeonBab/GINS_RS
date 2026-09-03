// Fill out your copyright notice in the Description page of Project Settings.

#include "RSCombatFunctionLibrary.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "RSGameplayTags.h"

#if !UE_BUILD_SHIPPING
namespace
{
	// 판정은 한 프레임만 실행되어 화면에 남는 것이 없으므로 확인용 출력을 콘솔에서 켜고 끕니다
	bool bRSHitCheckDebugEnabled = false;

	FAutoConsoleCommand RSToggleHitCheckDebugCommand(TEXT("RS.Combat.ToggleHitCheckDebug"), TEXT("Toggles hit check debug boxes and hit result logs."),
		FConsoleCommandDelegate::CreateLambda([]()
			{
				bRSHitCheckDebugEnabled = !bRSHitCheckDebugEnabled;

				UE_LOG(LogTemp, Log, TEXT("Hit check debug %s"), bRSHitCheckDebugEnabled ? TEXT("enabled") : TEXT("disabled"));
			}));
}
#endif

void URSCombatFunctionLibrary::FindTargetsInBox(const AActor* Attacker, ECollisionChannel TargetChannel, const FVector& BoxExtent, float ForwardOffset, TArray<AActor*>& OutTargets)
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

	// 축 정렬 박스를 사용하면 공격자가 대각선을 바라볼 때 판정이 어긋나므로 공격자의 회전을 함께 넘깁니다
	const FQuat BoxRotation = Attacker->GetActorQuat();
	const FVector BoxCenter = Attacker->GetActorLocation() + Attacker->GetActorForwardVector() * ForwardOffset;

	FCollisionQueryParams QueryParams;
	QueryParams.bTraceComplex = false;
	QueryParams.AddIgnoredActor(Attacker);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByChannel(Overlaps, BoxCenter, BoxRotation, TargetChannel, FCollisionShape::MakeBox(BoxExtent), QueryParams);

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

		OutTargets.Add(OverlappedActor);
	}

#if ENABLE_DRAW_DEBUG
	if (IsHitCheckDebugEnabled())
	{
		DrawDebugBox(World, BoxCenter, BoxExtent, BoxRotation, OutTargets.IsEmpty() ? FColor::Silver : FColor::Red, false, 1.0f);
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
