// Fill out your copyright notice in the Description page of Project Settings.


#include "RSGameplayAbility_BossGroggy.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"

URSGameplayAbility_BossGroggy::URSGameplayAbility_BossGroggy()
{
	// Montage 종료를 기다리는 동안 Task 참조를 유지해야 하므로 Actor별 인스턴스를 사용합니다
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void URSGameplayAbility_BossGroggy::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// Montage가 무력화 시간을 결정하므로 Montage가 없으면 무력화 구간이 성립하지 않습니다
	if (!ActorInfo || !GroggyMontage)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, GroggyMontage);
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleGroggyMontageFinished);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleGroggyMontageCancelled);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleGroggyMontageCancelled);
	MontageTask->ReadyForActivation();
}

void URSGameplayAbility_BossGroggy::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	EndAnimationGameplayStatesForMontage(ActorInfo, GroggyMontage);

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void URSGameplayAbility_BossGroggy::HandleGroggyMontageFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void URSGameplayAbility_BossGroggy::HandleGroggyMontageCancelled()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}
