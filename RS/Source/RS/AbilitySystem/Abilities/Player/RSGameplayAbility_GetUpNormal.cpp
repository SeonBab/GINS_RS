// Fill out your copyright notice in the Description page of Project Settings.

#include "RSGameplayAbility_GetUpNormal.h"

#include "RSGameplayTags.h"

URSGameplayAbility_GetUpNormal::URSGameplayAbility_GetUpNormal()
{
	// 입력이 아니라 누움 어빌리티의 대기 종료가 이 어빌리티를 실행합니다
	ActivationPolicy = ERSAbilityActivationPolicy::None;

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(RSGameplayTags::Ability_Recovery_GetUp_Normal);
	SetAssetTags(AssetTags);
}
