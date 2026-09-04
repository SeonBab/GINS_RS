// Fill out your copyright notice in the Description page of Project Settings.

#include "RSGameplayAbility_Skill.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "RSGameplayTags.h"

URSGameplayAbility_Skill::URSGameplayAbility_Skill()
{
	ActivationPolicy = ERSAbilityActivationPolicy::OnInputTriggered;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// 행동 잠금 구간이 끝난 뒤 들어온 입력부터 새 스킬을 활성화합니다
	ActivationBlockedTags.AddTag(RSGameplayTags::State_Action_Locked);
}

void URSGameplayAbility_Skill::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid() || !ActorInfo->GetAnimInstance() || !SkillMontage || SkillMontagePlayRate <= 0.0f)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	// 실행에 필요한 Context와 설정을 확인한 뒤 Commit하여 실패한 스킬이 쿨다운을 소비하지 않게 합니다
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, SkillMontage, SkillMontagePlayRate);
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleSkillMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleSkillMontageCancelled);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleSkillMontageCancelled);
	MontageTask->ReadyForActivation();
}

void URSGameplayAbility_Skill::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	EndAnimationGameplayStatesForMontage(ActorInfo, SkillMontage);

	// Commit에서 적용한 쿨다운은 스킬이 취소되어도 원래 만료 시점까지 유지합니다
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void URSGameplayAbility_Skill::HandleSkillMontageCompleted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void URSGameplayAbility_Skill::HandleSkillMontageCancelled()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}
