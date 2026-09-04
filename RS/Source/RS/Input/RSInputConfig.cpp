// Fill out your copyright notice in the Description page of Project Settings.


#include "RSInputConfig.h"

#include "InputAction.h"

const UInputAction* URSInputConfig::FindNativeInputAction(const FGameplayTag& InputTag, bool bLogNotFound) const
{
	for (const FRSInputAction& Action : NativeInputActions)
	{
		if (Action.InputAction && Action.InputTag.MatchesTagExact(InputTag))
		{
			return Action.InputAction;
		}
	}

	if (bLogNotFound)
	{
		UE_LOG(LogTemp, Error, TEXT("InputAction not found for InputTag [%s]"), *InputTag.ToString());
	}

	return nullptr;
}

const UInputAction* URSInputConfig::FindAbilityInputAction(const FGameplayTag& InputTag, bool bLogNotFound) const
{
	for (const FRSInputAction& Action : AbilityInputActions)
	{
		if (Action.InputAction && Action.InputTag.MatchesTagExact(InputTag))
		{
			return Action.InputAction;
		}
	}

	if (bLogNotFound)
	{
		UE_LOG(LogTemp, Error, TEXT("Ability InputAction not found for InputTag [%s]"), *InputTag.ToString());
	}

	return nullptr;
}
