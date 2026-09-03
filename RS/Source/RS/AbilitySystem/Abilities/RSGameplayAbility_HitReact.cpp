// Fill out your copyright notice in the Description page of Project Settings.

#include "RSGameplayAbility_HitReact.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "RSAbilitySystemComponent.h"
#include "RSGameplayTags.h"

URSGameplayAbility_HitReact::URSGameplayAbility_HitReact()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(RSGameplayTags::Ability_CrowdControl_HitReact);
	SetAssetTags(AssetTags);

	ActivationOwnedTags.AddTag(RSGameplayTags::State_CrowdControl_HitReact);

	// 경직 면역이 요청을 거부하는 지점이며, 공격은 요청만 하고 적용 여부는 대상이 정합니다
	ActivationBlockedTags.AddTag(RSGameplayTags::State_Immunity_HitReact);

	// 맞은 순간 진행 중이던 행동을 끊습니다
	CancelAbilitiesWithTag.AddTag(RSGameplayTags::Ability_Combat_BasicAttack);
	CancelAbilitiesWithTag.AddTag(RSGameplayTags::Ability_Movement_Dash);

	FAbilityTriggerData TriggerData;
	TriggerData.TriggerTag = RSGameplayTags::GameplayEvent_CrowdControl_HitReact;
	TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(TriggerData);
}

void URSGameplayAbility_HitReact::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid() || !HitReactMontage)
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

	// 경직 중 Navigation 경로 추종이 남아 캐릭터가 계속 미끄러지지 않게 현재 이동 요청을 중단합니다
	if (AController* Controller = Character->GetController())
	{
		Controller->StopMovement();
	}

	if (UCharacterMovementComponent* MovementComponent = Character->GetCharacterMovement())
	{
		MovementComponent->StopMovementImmediately();
	}

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, HitReactMontage);
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleHitReactMontageFinished);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleHitReactMontageCancelled);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleHitReactMontageCancelled);
	MontageTask->ReadyForActivation();
}

void URSGameplayAbility_HitReact::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (HitReactMontage && ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
	{
		if (URSAbilitySystemComponent* AbilitySystemComponent = Cast<URSAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get()))
		{
			// Montage 중단으로 Notify End를 받지 못한 경우에만 남아 있는 상태를 출처별로 정리합니다
			AbilitySystemComponent->EndAnimationGameplayStates(HitReactMontage);
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void URSGameplayAbility_HitReact::HandleHitReactMontageFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void URSGameplayAbility_HitReact::HandleHitReactMontageCancelled()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}
