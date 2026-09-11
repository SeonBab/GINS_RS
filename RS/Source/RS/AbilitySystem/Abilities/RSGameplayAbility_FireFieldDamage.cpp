#include "RSGameplayAbility_FireFieldDamage.h"

#include "RSGameplayTags.h"
#include "RSPlayerCharacter.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

URSGameplayAbility_FireFieldDamage::URSGameplayAbility_FireFieldDamage()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerExecution;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(RSGameplayTags::Ability_Combat_FireFieldDamage);
	SetAssetTags(AssetTags);

	FAbilityTriggerData TriggerData;
	TriggerData.TriggerTag = RSGameplayTags::GameplayEvent_Combat_FireFieldDamage;
	TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(TriggerData);
}

void URSGameplayAbility_FireFieldDamage::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	const ARSPlayerCharacter* TargetPlayer = TriggerEventData ? Cast<ARSPlayerCharacter>(TriggerEventData->Target.Get()) : nullptr;
	if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid() || !TargetPlayer || !DamageEffectClass)
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
	const float DamageAmount = Damage.GetValueAtLevel(GetAbilityLevel(Handle, ActorInfo));
	ApplyDamageToTarget(TargetActor, DamageEffectClass, DamageAmount);
	URSCombatFunctionLibrary::SendHitReaction(ActorInfo->AvatarActor.Get(), TargetActor, Reaction);

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

#if WITH_EDITOR
EDataValidationResult URSGameplayAbility_FireFieldDamage::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult ValidationResult = Super::IsDataValid(Context);

	if (!DamageEffectClass)
	{
		Context.AddError(FText::FromString(TEXT("DamageEffectClass is not configured.")));
		ValidationResult = EDataValidationResult::Invalid;
	}

	if (!URSCombatFunctionLibrary::ValidateIntegerDamage(Damage, TEXT("Damage"), Context))
	{
		ValidationResult = EDataValidationResult::Invalid;
	}

	return ValidationResult;
}
#endif
