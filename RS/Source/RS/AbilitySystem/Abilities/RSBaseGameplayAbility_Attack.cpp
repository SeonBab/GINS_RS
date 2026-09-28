// Fill out your copyright notice in the Description page of Project Settings.

#include "RSBaseGameplayAbility_Attack.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Combat/RSCombatFunctionLibrary.h"
#include "GameplayEffect.h"
#include "RSGameplayTags.h"
#include "RSHealthSet.h"

void URSBaseGameplayAbility_Attack::ApplyDamageToTarget(AActor* TargetActor, TSubclassOf<UGameplayEffect> DamageEffectClass, float DamageAmount)
{
	if (!TargetActor || !CurrentActorInfo || !CurrentActorInfo->AbilitySystemComponent.IsValid())
	{
		return;
	}

	UAbilitySystemComponent* TargetAbilitySystemComp = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor);
	if (!TargetAbilitySystemComp)
	{
		return;
	}

	if (!DamageEffectClass)
	{
		return;
	}

	const float AbilityLevel = GetAbilityLevel(CurrentSpecHandle, CurrentActorInfo);
	FGameplayEffectSpecHandle DamageSpecHandle = MakeOutgoingGameplayEffectSpec(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, DamageEffectClass, AbilityLevel);
	if (!DamageSpecHandle.IsValid())
	{
		return;
	}

	// 공용 대미지 GameplayEffect에 피해량을 고정하지 않고 이번 타격의 값을 실행별로 설정합니다
	DamageSpecHandle.Data->SetSetByCallerMagnitude(RSGameplayTags::SetByCaller_Damage, DamageAmount);
	CurrentActorInfo->AbilitySystemComponent->ApplyGameplayEffectSpecToTarget(*DamageSpecHandle.Data.Get(), TargetAbilitySystemComp);

	if (URSCombatFunctionLibrary::IsHitCheckDebugEnabled())
	{
		const URSHealthSet* TargetHealthSet = TargetAbilitySystemComp->GetSet<URSHealthSet>();
		UE_LOG(LogTemp, Log, TEXT("%s applied %.1f damage to %s, remaining health %.1f"), *GetName(), DamageAmount, *GetNameSafe(TargetActor), TargetHealthSet ? TargetHealthSet->GetHealth() : -1.0f);
	}
}
