// Fill out your copyright notice in the Description page of Project Settings.

#include "RSBaseGameplayAbility.h"

#include "RSGameplayTags.h"

URSBaseGameplayAbility::URSBaseGameplayAbility()
{
	ActivationBlockedTags.AddTag(RSGameplayTags::State_Dead);
}

ERSAbilityActivationPolicy URSBaseGameplayAbility::GetActivationPolicy() const
{
	return ActivationPolicy;
}
