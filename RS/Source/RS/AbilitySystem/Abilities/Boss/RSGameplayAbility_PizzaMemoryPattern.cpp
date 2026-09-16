#include "RSGameplayAbility_PizzaMemoryPattern.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

URSGameplayAbility_PizzaMemoryPattern::URSGameplayAbility_PizzaMemoryPattern()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void URSGameplayAbility_PizzaMemoryPattern::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	UE_LOG(LogTemp, Warning, TEXT("%s has no runtime flow yet and was cancelled."), *GetName());
	EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
}

#if WITH_EDITOR
EDataValidationResult URSGameplayAbility_PizzaMemoryPattern::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult ValidationResult = Super::IsDataValid(Context);

	FString ValidationError;
	if (!PizzaMemoryPatternDefinition.IsDataValid(&ValidationError))
	{
		Context.AddError(FText::FromString(FString::Printf(TEXT("PizzaMemoryPatternDefinition is invalid: %s"), *ValidationError)));
		ValidationResult = EDataValidationResult::Invalid;
	}

	return ValidationResult;
}
#endif
