// Fill out your copyright notice in the Description page of Project Settings.

#if WITH_TESTS

#include "RSDamageImmunityFeedbackTestTypes.h"

#include "RSGameplayTags.h"

URSDamageImmunityFeedbackTestCooldownEffect::URSDamageImmunityFeedbackTestCooldownEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	FSetByCallerFloat DurationSetByCaller;
	DurationSetByCaller.DataTag = RSGameplayTags::SetByCaller_Cooldown_Duration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(DurationSetByCaller);
}

URSDamageImmunityFeedbackTestAbility::URSDamageImmunityFeedbackTestAbility()
{
	// 실제 Blueprint가 Class Defaults에서 채우는 값을 테스트에서는 생성자로 대신 채웁니다
	CooldownGameplayEffectClass = URSDamageImmunityFeedbackTestCooldownEffect::StaticClass();
	CooldownDuration = 1.0f;
	CooldownTags.AddTag(RSGameplayTags::Cooldown_Ability_DamageImmunityFeedback);
}

#endif // WITH_TESTS
