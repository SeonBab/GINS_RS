// Fill out your copyright notice in the Description page of Project Settings.

#include "RSBaseGameplayAbility_GetUp.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "GameFramework/Character.h"
#include "RSGameplayTags.h"

URSBaseGameplayAbility_GetUp::URSBaseGameplayAbility_GetUp()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	ActivationOwnedTags.AddTag(RSGameplayTags::State_CrowdControl_GettingUp);

	// 일반 기상은 누워 있을 때만 실행하며 빠른 기상은 파생 생성자에서 이 조건을 가용 구간 태그로 교체합니다
	ActivationRequiredTags.AddTag(RSGameplayTags::State_CrowdControl_Downed);

	// 누움이 부여한 State.Action.Locked는 기상을 막으면 안 되므로 이 어빌리티는 공통 잠금을 차단 조건에 넣지 않습니다
}

void URSBaseGameplayAbility_GetUp::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid() || !GetUpMontage)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
	if (!Character)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	StartGetUpMovement(*Character);

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, GetUpMontage);
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleGetUpMontageFinished);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleGetUpMontageCancelled);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleGetUpMontageCancelled);
	MontageTask->ReadyForActivation();
	if (!IsActive())
	{
		return;
	}

	// 새 Montage가 슬롯 소유권을 넘겨받은 뒤 이전 상태를 취소해 두 Montage 사이에 기본 자세가 노출되지 않게 합니다
	// Commit과 Montage 재생이 모두 성공한 뒤 취소하므로 실패한 기상이 누운 상태를 먼저 지우지 않습니다
	FGameplayTagContainer PreviousRecoveryAbilityTags;
	PreviousRecoveryAbilityTags.AddTag(RSGameplayTags::Ability_CrowdControl_Knockdown);
	PreviousRecoveryAbilityTags.AddTag(RSGameplayTags::Ability_CrowdControl_Downed);
	ActorInfo->AbilitySystemComponent->CancelAbilities(&PreviousRecoveryAbilityTags, nullptr, this);
}

void URSBaseGameplayAbility_GetUp::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	EndAnimationGameplayStatesForMontage(ActorInfo, GetUpMontage);

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void URSBaseGameplayAbility_GetUp::StartGetUpMovement(ACharacter& Character)
{
	// 일반 기상은 제자리에서 일어나므로 기반에서는 아무 이동도 하지 않습니다
}

void URSBaseGameplayAbility_GetUp::HandleGetUpMontageFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void URSBaseGameplayAbility_GetUp::HandleGetUpMontageCancelled()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}
