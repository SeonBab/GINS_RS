// Fill out your copyright notice in the Description page of Project Settings.

#include "RSBaseGameplayAbility.h"

#include "GameplayEffect.h"
#include "RSGameplayTags.h"

URSBaseGameplayAbility::URSBaseGameplayAbility()
{
	ActivationBlockedTags.AddTag(RSGameplayTags::State_Dead);
}

ERSAbilityActivationPolicy URSBaseGameplayAbility::GetActivationPolicy() const
{
	return ActivationPolicy;
}

const FGameplayTagContainer* URSBaseGameplayAbility::GetCooldownTags() const
{
	// 공용 GameplayEffect에는 특정 Ability의 Tag를 고정하지 않고 Ability 설정을 활성화 차단 기준으로 사용합니다
	if (!CooldownTags.IsEmpty())
	{
		return &CooldownTags;
	}

	// 프로젝트 공통 쿨다운 설정이 없는 Ability는 Unreal의 기본 CooldownGameplayEffect 방식을 유지합니다
	return Super::GetCooldownTags();
}

void URSBaseGameplayAbility::ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	// 쿨다운을 사용하는 Ability는 활성화 직후 CommitAbility를 호출하며 이 함수가 실행되는 시점부터 쿨다운이 시작됩니다
	if (CooldownTags.IsEmpty())
	{
		Super::ApplyCooldown(Handle, ActorInfo, ActivationInfo);

		return;
	}

	UGameplayEffect* CooldownGameplayEffect = GetCooldownGameplayEffect();
	if (!ensureMsgf(CooldownGameplayEffect, TEXT("%s에 CooldownTags가 설정되었지만 CooldownGameplayEffectClass가 없습니다"), *GetName()))
	{
		return;
	}

	const float AbilityLevel = GetAbilityLevel(Handle, ActorInfo);
	const float Duration = CooldownDuration.GetValueAtLevel(AbilityLevel);
	if (!ensureMsgf(Duration > 0.0f, TEXT("%s의 CooldownDuration은 0보다 커야 합니다"), *GetName()))
	{
		return;
	}

	// 공유 GameplayEffect 정의를 수정하지 않고 실행별 Spec에 각 Ability의 시간과 Tag를 기록합니다
	FGameplayEffectSpecHandle CooldownSpecHandle = MakeOutgoingGameplayEffectSpec(Handle, ActorInfo, ActivationInfo, CooldownGameplayEffect->GetClass(), AbilityLevel);
	if (!ensureMsgf(CooldownSpecHandle.IsValid(), TEXT("%s의 쿨다운 GameplayEffectSpec을 생성하지 못했습니다"), *GetName()))
	{
		return;
	}

	// Active GameplayEffect가 Tag 수명을 소유하므로 만료 시 별도 Timer나 수동 제거 없이 함께 정리됩니다
	CooldownSpecHandle.Data->DynamicGrantedTags.AppendTags(CooldownTags);
	CooldownSpecHandle.Data->SetSetByCallerMagnitude(RSGameplayTags::SetByCaller_Cooldown_Duration, Duration);

	ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, CooldownSpecHandle);
}
