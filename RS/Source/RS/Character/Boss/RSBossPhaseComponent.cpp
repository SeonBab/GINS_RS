// Fill out your copyright notice in the Description page of Project Settings.


#include "RSBossPhaseComponent.h"

#include "GameFramework/Pawn.h"
#include "Abilities/RSBaseGameplayAbility.h"

namespace
{
	/** 로그에서 사이클 진행을 추적할 수 있도록 패턴 종류의 이름을 반환합니다 */
	FString GetPatternTypeName(ERSBossPatternType PatternType)
	{
		return UEnum::GetDisplayValueAsText(PatternType).ToString();
	}
}

URSBossPhaseComponent::URSBossPhaseComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// 기획이 확정한 상시 두 번 뒤 특수 한 번을 기본값으로 두고 에셋에서 조정합니다
	CycleSequence = { ERSBossPatternType::Basic, ERSBossPatternType::Basic, ERSBossPatternType::Special };
}

URSBossPhaseComponent* URSBossPhaseComponent::FindPhaseComponent(const APawn* Pawn)
{
	// Behavior Tree 노드가 보스 캐릭터 타입을 알지 않아도 진행 컴포넌트를 찾을 수 있게 합니다
	return Pawn ? Pawn->FindComponentByClass<URSBossPhaseComponent>() : nullptr;
}

bool URSBossPhaseComponent::TryGetCurrentPatternType(ERSBossPatternType& OutPatternType) const
{
	if (!CycleSequence.IsValidIndex(CurrentCycleIndex))
	{
		// 사이클이 비어 있으면 어떤 차례도 성립하지 않으므로 런타임 상황이 아니라 에셋 설정 누락입니다
		UE_LOG(LogTemp, Warning, TEXT("[BossPhase] %s의 CycleSequence가 비어 있어 현재 차례를 결정할 수 없습니다"), *GetNameSafe(GetOwner()));

		return false;
	}

	OutPatternType = CycleSequence[CurrentCycleIndex];

	return true;
}

bool URSBossPhaseComponent::TrySelectPattern(ERSBossPatternType PatternType, TSubclassOf<URSBaseGameplayAbility>& OutAbilityClass) const
{
	const TArray<TSubclassOf<URSBaseGameplayAbility>>& Candidates = GetPatternCandidates(PatternType);

	// 후보가 없는 것은 정상적인 실행 실패가 아니라 에셋 설정 누락입니다
	if (Candidates.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[BossPhase] %s의 %s 패턴 후보 목록이 비어 있습니다"), *GetNameSafe(GetOwner()), *GetPatternTypeName(PatternType));

		return false;
	}

	// 1단계에서는 직전 패턴 제외와 쿨다운 필터를 두지 않으므로 후보 중 하나를 그대로 고릅니다
	const int32 SelectedIndex = FMath::RandRange(0, Candidates.Num() - 1);
	const TSubclassOf<URSBaseGameplayAbility> SelectedAbilityClass = Candidates[SelectedIndex];
	if (!SelectedAbilityClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BossPhase] %s의 %s 패턴 후보 %d번이 비어 있습니다"), *GetNameSafe(GetOwner()), *GetPatternTypeName(PatternType), SelectedIndex);

		return false;
	}

	OutAbilityClass = SelectedAbilityClass;

	UE_LOG(LogTemp, Log, TEXT("[BossPhase] Select %s %s"), *GetPatternTypeName(PatternType), *GetNameSafe(SelectedAbilityClass));

	return true;
}

void URSBossPhaseComponent::AdvancePatternCycle()
{
	if (CycleSequence.IsEmpty())
	{
		return;
	}

	const int32 PreviousCycleIndex = CurrentCycleIndex;
	CurrentCycleIndex = (CurrentCycleIndex + 1) % CycleSequence.Num();

	UE_LOG(LogTemp, Log, TEXT("[BossPhase] Advance %d -> %d"), PreviousCycleIndex, CurrentCycleIndex);
}

void URSBossPhaseComponent::ResetPatternCycle()
{
	CurrentCycleIndex = 0;

	UE_LOG(LogTemp, Log, TEXT("[BossPhase] Reset"));
}

#if WITH_DEV_AUTOMATION_TESTS
void URSBossPhaseComponent::SetPatternCycleForTest(const TArray<ERSBossPatternType>& InCycleSequence, const TArray<TSubclassOf<URSBaseGameplayAbility>>& InBasicPatterns, const TArray<TSubclassOf<URSBaseGameplayAbility>>& InSpecialPatterns)
{
	CycleSequence = InCycleSequence;
	BasicPatterns = InBasicPatterns;
	SpecialPatterns = InSpecialPatterns;
	CurrentCycleIndex = 0;
}
#endif

const TArray<TSubclassOf<URSBaseGameplayAbility>>& URSBossPhaseComponent::GetPatternCandidates(ERSBossPatternType PatternType) const
{
	return PatternType == ERSBossPatternType::Special ? SpecialPatterns : BasicPatterns;
}
