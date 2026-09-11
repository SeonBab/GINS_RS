// Fill out your copyright notice in the Description page of Project Settings.

#include "RSFallingRocksTestTypes.h"

URSFallingRocksTestAbility::URSFallingRocksTestAbility()
{
	FallingRocksDefinition.RockInterval = 0.05f;
	FallingRocksDefinition.ImpactDelay = 0.5f;
	FallingRocksDefinition.FallEffectLeadTime = 0.0f;
}

void URSFallingRocksTestAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	++ActivationCount;

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}
