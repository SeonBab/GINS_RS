// Fill out your copyright notice in the Description page of Project Settings.


#include "RSCheatManager.h"

#include "EngineUtils.h"
#include "RSAbilitySystemComponent.h"
#include "RSBossEncounter.h"
#include "RSGameplayTags.h"
#include "RSPlayerState.h"

void URSCheatManager::RS_ToggleDownedTag()
{
	const APlayerController* PlayerController = GetPlayerController();
	const ARSPlayerState* RSPlayerState = PlayerController ? PlayerController->GetPlayerState<ARSPlayerState>() : nullptr;
	URSAbilitySystemComponent* AbilitySystemComp = RSPlayerState ? RSPlayerState->GetRSAbilitySystemComponent() : nullptr;
	if (!AbilitySystemComp)
	{
		return;
	}

	// 어빌리티가 부여한 태그를 실수로 제거하지 않도록 이 치트가 직접 부여한 경우에만 제거합니다
	if (bDownedTagApplied)
	{
		AbilitySystemComp->RemoveLooseGameplayTag(RSGameplayTags::State_CrowdControl_Downed);
	}
	else
	{
		AbilitySystemComp->AddLooseGameplayTag(RSGameplayTags::State_CrowdControl_Downed);
	}

	bDownedTagApplied = !bDownedTagApplied;
}

void URSCheatManager::RS_CompleteBossEncounter()
{
	ARSBossEncounter* BossEncounter = FindTargetBossEncounter();
	if (!BossEncounter)
	{
		return;
	}

	// 시작되지 않은 Encounter를 완료시키면 아무 이벤트 없이 이후 참가자 등록이 모두 거부됩니다
	if (!BossEncounter->IsEncounterActive())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s is not active"), *GetNameSafe(BossEncounter));
		return;
	}

	BossEncounter->CompleteEncounter();
}

void URSCheatManager::RS_ExpireBossTimeLimit()
{
	ARSBossEncounter* BossEncounter = FindTargetBossEncounter();
	if (!BossEncounter)
	{
		return;
	}

	BossEncounter->ForceExpireTimeLimit();
}

ARSBossEncounter* URSCheatManager::FindTargetBossEncounter() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	ARSBossEncounter* FallbackEncounter = nullptr;

	for (TActorIterator<ARSBossEncounter> EncounterIterator(World); EncounterIterator; ++EncounterIterator)
	{
		ARSBossEncounter* BossEncounter = *EncounterIterator;

		if (BossEncounter->IsEncounterActive())
		{
			return BossEncounter;
		}

		if (!FallbackEncounter)
		{
			FallbackEncounter = BossEncounter;
		}
	}

	// 대상을 찾지 못한 이유를 알 수 있도록 월드에서 액터를 찾는 명령에서는 실패를 알립니다
	if (!FallbackEncounter)
	{
		UE_LOG(LogTemp, Warning, TEXT("No ARSBossEncounter in world"));
	}

	return FallbackEncounter;
}
