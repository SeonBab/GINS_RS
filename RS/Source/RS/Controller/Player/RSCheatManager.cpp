// Fill out your copyright notice in the Description page of Project Settings.


#include "RSCheatManager.h"

#include "EngineUtils.h"
#include "GameplayEffect.h"
#include "RSAbilitySystemComponent.h"
#include "RSBossCharacter.h"
#include "RSBossEncounter.h"
#include "RSGameplayTags.h"
#include "RSPlayerState.h"
#include "RSHealthSet.h"
#include "RSBossPhaseComponent.h"
#include "RSBaseGameplayAbility.h"

URSAbilitySystemComponent* URSCheatManager::FindPlayerAbilitySystemComponent() const
{
	const APlayerController* PlayerController = GetPlayerController();
	const ARSPlayerState* PlayerState = PlayerController ? PlayerController->GetPlayerState<ARSPlayerState>() : nullptr;
	URSAbilitySystemComponent* AbilitySystemComp = PlayerState ? PlayerState->GetRSAbilitySystemComponent() : nullptr;
	if (!AbilitySystemComp || !IsValid(AbilitySystemComp->GetAvatarActor()))
	{
		UE_LOG(LogTemp, Warning, TEXT("No player AbilitySystemComponent with a live avatar"));
		return nullptr;
	}
	return AbilitySystemComp;
}

void URSCheatManager::ApplyPlayerHealthEffect(int32 Amount, bool bHealing)
{
	URSAbilitySystemComponent* AbilitySystemComp = FindPlayerAbilitySystemComponent();
	if (!AbilitySystemComp)
	{
		return;
	}
	if (!AbilitySystemComp->GetAvatarActor()->HasAuthority() || Amount <= 0 || AbilitySystemComp->HasMatchingGameplayTag(RSGameplayTags::State_Dead))
	{
		UE_LOG(LogTemp, Warning, TEXT("Health cheats require authority, a living player and a positive amount"));
		return;
	}
	const TCHAR* EffectPath = bHealing
		? TEXT("/Game/AbilitySystem/GameplayEffects/GE_Heal_Basic.GE_Heal_Basic_C")
		: TEXT("/Game/AbilitySystem/GameplayEffects/GE_Damage_Basic.GE_Damage_Basic_C");
	const TSubclassOf<UGameplayEffect> EffectClass = LoadClass<UGameplayEffect>(nullptr, EffectPath);
	if (!EffectClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to load %s"), EffectPath);
		return;
	}
	const float PreviousHealth = AbilitySystemComp->GetNumericAttribute(URSHealthSet::GetHealthAttribute());
	const FGameplayEffectSpecHandle Spec = AbilitySystemComp->MakeOutgoingSpec(EffectClass, 1.0f, AbilitySystemComp->MakeEffectContext());
	if (!Spec.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to create health GameplayEffectSpec"));
		return;
	}
	Spec.Data->SetSetByCallerMagnitude(bHealing ? RSGameplayTags::SetByCaller_Healing : RSGameplayTags::SetByCaller_Damage, static_cast<float>(Amount));
	AbilitySystemComp->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	UE_LOG(LogTemp, Log, TEXT("Player %s %d: Health %.0f -> %.0f"), bHealing ? TEXT("Heal") : TEXT("Damage"), Amount, PreviousHealth, AbilitySystemComp->GetNumericAttribute(URSHealthSet::GetHealthAttribute()));
}

void URSCheatManager::DamagePlayer(int32 Amount)
{
	ApplyPlayerHealthEffect(Amount, false);
}

void URSCheatManager::HealPlayer(int32 Amount)
{
	ApplyPlayerHealthEffect(Amount, true);
}

void URSCheatManager::KillPlayer()
{
	URSAbilitySystemComponent* AbilitySystemComp = FindPlayerAbilitySystemComponent();
	if (!AbilitySystemComp || !AbilitySystemComp->GetAvatarActor()->HasAuthority())
	{
		return;
	}
	// 무적 상태에서도 사망을 테스트할 수 있도록 피해 대신 Health를 직접 0으로 설정합니다
	// 사망 태그와 보스전 실패 처리는 체력 변경을 감시하는 기존 코드에 맡깁니다
	AbilitySystemComp->SetNumericAttributeBase(URSHealthSet::GetHealthAttribute(), 0.0f);
	UE_LOG(LogTemp, Log, TEXT("Player Health set to zero"));
}

void URSCheatManager::ClearPlayerInvulnerability()
{
	if (URSAbilitySystemComponent* AbilitySystemComp = InvulnerableAbilitySystemComp.Get())
	{
		AbilitySystemComp->RemoveLooseGameplayTag(RSGameplayTags::State_Immunity_Damage);
	}
	InvulnerableAbilitySystemComp.Reset();
}

void URSCheatManager::BeginDestroy()
{
	ClearPlayerInvulnerability();
	Super::BeginDestroy();
}

void URSCheatManager::SetPlayerInvulnerable(bool bEnabled)
{
	URSAbilitySystemComponent* AbilitySystemComp = FindPlayerAbilitySystemComponent();
	if (!AbilitySystemComp || !AbilitySystemComp->GetAvatarActor()->HasAuthority())
	{
		return;
	}
	if (!bEnabled || InvulnerableAbilitySystemComp.Get() != AbilitySystemComp)
	{
		ClearPlayerInvulnerability();
	}
	if (bEnabled && !InvulnerableAbilitySystemComp.IsValid())
	{
		AbilitySystemComp->AddLooseGameplayTag(RSGameplayTags::State_Immunity_Damage);
		InvulnerableAbilitySystemComp = AbilitySystemComp;
	}
	UE_LOG(LogTemp, Log, TEXT("Player cheat damage immunity %s; other immunity sources are preserved"), bEnabled ? TEXT("enabled") : TEXT("disabled"));
}

void URSCheatManager::DumpState() const
{
	auto PrintAbilitySystem = [](const TCHAR* Label, URSAbilitySystemComponent* AbilitySystemComp)
	{
		if (!AbilitySystemComp)
		{
			return;
		}
		FGameplayTagContainer Tags;
		AbilitySystemComp->GetOwnedGameplayTags(Tags);
		UE_LOG(LogTemp, Log, TEXT("%s %s Health %.0f/%.0f Tags: %s"), Label, *GetNameSafe(AbilitySystemComp->GetAvatarActor()), AbilitySystemComp->GetNumericAttribute(URSHealthSet::GetHealthAttribute()), AbilitySystemComp->GetNumericAttribute(URSHealthSet::GetMaxHealthAttribute()), *Tags.ToStringSimple());
		for (const FGameplayAbilitySpec& Spec : AbilitySystemComp->GetActivatableAbilities())
		{
			if (Spec.IsActive())
			{
				UE_LOG(LogTemp, Log, TEXT("  Active Ability: %s"), *GetNameSafe(Spec.Ability));
			}
		}
	};
	PrintAbilitySystem(TEXT("Player"), FindPlayerAbilitySystemComponent());
	for (TActorIterator<ARSBossCharacter> It(GetWorld()); It; ++It)
	{
		PrintAbilitySystem(TEXT("Boss"), It->GetRSAbilitySystemComponent());
		if (const URSBossPhaseComponent* Phase = URSBossPhaseComponent::FindPhaseComponent(*It))
		{
			UE_LOG(LogTemp, Log, TEXT("  Phase index %d Cycle index %d"), Phase->GetCurrentPhaseIndex(), Phase->GetCurrentCycleIndex());
		}
	}
	if (const ARSBossEncounter* Encounter = FindTargetBossEncounter())
	{
		UE_LOG(LogTemp, Log, TEXT("Encounter %s State %s Result %s Remaining %.2fs Hits %d"), *GetNameSafe(Encounter), *UEnum::GetValueAsString(Encounter->GetEncounterState()), *UEnum::GetValueAsString(Encounter->GetEncounterResult()), Encounter->GetRemainingTimeSeconds(), Encounter->GetHitCount());
	}
}

void URSCheatManager::RunBossPattern(const FString& PatternName)
{
	ARSBossCharacter* Target = nullptr;
	for (TActorIterator<ARSBossCharacter> It(GetWorld()); It; ++It)
	{
		if (Target)
		{
			UE_LOG(LogTemp, Warning, TEXT("RunPattern requires exactly one boss in the world"));
			return;
		}
		Target = *It;
	}
	URSBossPhaseComponent* Phase = URSBossPhaseComponent::FindPhaseComponent(Target);
	if (!Phase)
	{
		UE_LOG(LogTemp, Warning, TEXT("No boss phase component"));
		return;
	}
#if !UE_BUILD_SHIPPING
	Phase->QueueDebugPattern(PatternName);
#endif
}

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
