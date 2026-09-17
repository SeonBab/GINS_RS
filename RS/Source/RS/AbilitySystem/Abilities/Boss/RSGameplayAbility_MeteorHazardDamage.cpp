#include "RSGameplayAbility_MeteorHazardDamage.h"

#include "Combat/RSCombatFunctionLibrary.h"
#include "RSGameplayTags.h"
#include "RSPlayerCharacter.h"
#include "GameplayEffect.h"

URSGameplayAbility_MeteorHazardDamage::URSGameplayAbility_MeteorHazardDamage()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerExecution;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(RSGameplayTags::Ability_Combat_MeteorHazardDamage);
	SetAssetTags(AssetTags);

	FAbilityTriggerData TriggerData;
	TriggerData.TriggerTag = RSGameplayTags::GameplayEvent_Combat_MeteorHazardDamage;
	TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(TriggerData);
}

void URSGameplayAbility_MeteorHazardDamage::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	const ARSPlayerCharacter* TargetPlayer = TriggerEventData ? Cast<ARSPlayerCharacter>(TriggerEventData->Target.Get()) : nullptr;
	const UGameplayEffect* DamageEffect = TriggerEventData ? Cast<UGameplayEffect>(TriggerEventData->OptionalObject.Get()) : nullptr;
	const float DamageAmount = TriggerEventData ? TriggerEventData->EventMagnitude : 0.0f;
	if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid() || !TargetPlayer || !DamageEffect || !FMath::IsFinite(DamageAmount) || DamageAmount <= 0.0f)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	AActor* TargetActor = const_cast<ARSPlayerCharacter*>(TargetPlayer);
	ApplyDamageToTarget(TargetActor, DamageEffect->GetClass(), DamageAmount);

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
