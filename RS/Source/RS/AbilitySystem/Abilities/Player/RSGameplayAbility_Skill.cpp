// Fill out your copyright notice in the Description page of Project Settings.

#include "RSGameplayAbility_Skill.h"

URSGameplayAbility_Skill::URSGameplayAbility_Skill()
{
	ActivationPolicy = ERSAbilityActivationPolicy::OnInputTriggered;
}

UAnimMontage* URSGameplayAbility_Skill::GetAttackMontage() const
{
	return SkillMontage;
}

float URSGameplayAbility_Skill::GetAttackMontagePlayRate() const
{
	return SkillMontagePlayRate;
}
