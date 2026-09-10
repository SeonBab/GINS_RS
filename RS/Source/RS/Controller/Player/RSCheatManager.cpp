// Fill out your copyright notice in the Description page of Project Settings.


#include "RSCheatManager.h"

#include "EngineUtils.h"
#include "GameplayEffect.h"
#include "RSAbilitySystemComponent.h"
#include "RSBossCharacter.h"
#include "RSBossEncounter.h"
#include "RSGameplayTags.h"
#include "RSPlayerState.h"

namespace RSCheatDamage
{
	// 치트는 실제 전투와 같은 대미지 GameplayEffect를 사용해야 통지와 표시 경로까지 함께 검증됩니다
	const TCHAR* DamageEffectPath = TEXT("/Game/AbilitySystem/GameplayEffects/GE_Damage_Basic.GE_Damage_Basic_C");
}

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
		AbilitySystemComp->RemoveLooseGameplayTag(RSGameplayTags::State_Recovery_QuickGetUpAvailable);
	}
	else
	{
		AbilitySystemComp->AddLooseGameplayTag(RSGameplayTags::State_CrowdControl_Downed);
		AbilitySystemComp->AddLooseGameplayTag(RSGameplayTags::State_Recovery_QuickGetUpAvailable);
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

	BossEncounter->ResolveEncounter(ERSBossEncounterResult::Clear);
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

void URSCheatManager::RS_DamageBoss(int32 Amount)
{
	// 음수와 0은 HealthSet에서 걸러져 아무 일도 일어나지 않으므로 조용한 무반응 대신 이유를 남깁니다
	if (Amount <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("RS_DamageBoss needs a positive amount but received %d"), Amount);
		return;
	}

	const APlayerController* PlayerController = GetPlayerController();
	const ARSPlayerState* RSPlayerState = PlayerController ? PlayerController->GetPlayerState<ARSPlayerState>() : nullptr;
	URSAbilitySystemComponent* InstigatorAbilitySystemComp = RSPlayerState ? RSPlayerState->GetRSAbilitySystemComponent() : nullptr;
	if (!InstigatorAbilitySystemComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("RS_DamageBoss found no player AbilitySystemComponent"));
		return;
	}

	UWorld* World = GetWorld();
	URSAbilitySystemComponent* TargetAbilitySystemComp = nullptr;
	if (World)
	{
		for (TActorIterator<ARSBossCharacter> BossIterator(World); BossIterator; ++BossIterator)
		{
			TargetAbilitySystemComp = BossIterator->GetRSAbilitySystemComponent();
			if (TargetAbilitySystemComp)
			{
				break;
			}
		}
	}

	if (!TargetAbilitySystemComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("RS_DamageBoss found no boss AbilitySystemComponent"));
		return;
	}

	const TSubclassOf<UGameplayEffect> DamageEffectClass = LoadClass<UGameplayEffect>(nullptr, RSCheatDamage::DamageEffectPath);
	if (!DamageEffectClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("RS_DamageBoss failed to load %s"), RSCheatDamage::DamageEffectPath);
		return;
	}

	const FGameplayEffectSpecHandle DamageSpecHandle = InstigatorAbilitySystemComp->MakeOutgoingSpec(DamageEffectClass, 1.0f, InstigatorAbilitySystemComp->MakeEffectContext());
	if (!DamageSpecHandle.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("RS_DamageBoss failed to create a damage GameplayEffectSpec"));
		return;
	}

	DamageSpecHandle.Data->SetSetByCallerMagnitude(RSGameplayTags::SetByCaller_Damage, static_cast<float>(Amount));

	InstigatorAbilitySystemComp->ApplyGameplayEffectSpecToTarget(*DamageSpecHandle.Data.Get(), TargetAbilitySystemComp);
}
