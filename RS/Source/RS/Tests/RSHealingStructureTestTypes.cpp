// Fill out your copyright notice in the Description page of Project Settings.

#if WITH_TESTS

#include "RSHealingStructureTestTypes.h"

#include "RSGameplayTags.h"
#include "RSHealthSet.h"

URSHealingStructureTestHealEffect::URSHealingStructureTestHealEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FSetByCallerFloat HealingSetByCaller;
	HealingSetByCaller.DataTag = RSGameplayTags::SetByCaller_Healing;

	// Health는 HideFromModifiers라 직접 수정할 수 없고 Healing을 거쳐 URSHealthSet이 반영합니다
	FGameplayModifierInfo HealingModifier;
	HealingModifier.Attribute = URSHealthSet::GetHealingAttribute();
	HealingModifier.ModifierOp = EGameplayModOp::Additive;
	HealingModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(HealingSetByCaller);
	Modifiers.Add(HealingModifier);
}

#endif // WITH_TESTS
