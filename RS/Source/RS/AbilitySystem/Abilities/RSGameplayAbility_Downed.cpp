// Fill out your copyright notice in the Description page of Project Settings.

#include "RSGameplayAbility_Downed.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "RSAbilitySystemComponent.h"
#include "RSGameplayTags.h"

URSGameplayAbility_Downed::URSGameplayAbility_Downed()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(RSGameplayTags::Ability_CrowdControl_Downed);
	SetAssetTags(AssetTags);

	// 누움의 수명은 Montage 길이가 아니라 이 어빌리티가 정하므로 행동 잠금도 함께 소유합니다
	ActivationOwnedTags.AddTag(RSGameplayTags::State_CrowdControl_Downed);
	ActivationOwnedTags.AddTag(RSGameplayTags::State_Action_Locked);
}

void URSGameplayAbility_Downed::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid() || !DownedMontage || AutoGetUpDelay <= 0.0f)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	// 넉백으로 남은 속도가 누운 채 미끄러지게 만들지 않도록 정지시킵니다
	if (const ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get()))
	{
		if (UCharacterMovementComponent* MovementComponent = Character->GetCharacterMovement())
		{
			MovementComponent->StopMovementImmediately();
		}
	}

	// 반복 재생이므로 완료 알림이 오지 않으며, 누움의 수명은 아래 대기와 외부 취소가 결정합니다
	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, DownedMontage);
	MontageTask->ReadyForActivation();

	UAbilityTask_WaitDelay* AutoGetUpTask = UAbilityTask_WaitDelay::WaitDelay(this, AutoGetUpDelay);
	AutoGetUpTask->OnFinish.AddDynamic(this, &ThisClass::HandleAutoGetUpDelayFinished);
	AutoGetUpTask->ReadyForActivation();
}

void URSGameplayAbility_Downed::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (DownedMontage && ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
	{
		if (URSAbilitySystemComponent* AbilitySystemComponent = Cast<URSAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get()))
		{
			// Montage 중단으로 Notify End를 받지 못한 경우에만 남아 있는 상태를 출처별로 정리합니다
			AbilitySystemComponent->EndAnimationGameplayStates(DownedMontage);
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void URSGameplayAbility_Downed::HandleAutoGetUpDelayFinished()
{
	UAbilitySystemComponent* AbilitySystemComponent = CurrentActorInfo ? CurrentActorInfo->AbilitySystemComponent.Get() : nullptr;

	// 기상은 누운 상태를 요구하므로 이 어빌리티가 살아 있는 동안 활성화해야 하며, 성공하면 기상이 누움을 취소합니다
	if (TryActivateAbilityByClass(AbilitySystemComponent, NormalGetUpAbilityClass))
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("%s could not activate %s after the downed delay"), *GetName(), *GetNameSafe(NormalGetUpAbilityClass));

	// 일어나지 못했더라도 캐릭터가 누운 채 잠겨 남지 않도록 상태를 해제합니다
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
