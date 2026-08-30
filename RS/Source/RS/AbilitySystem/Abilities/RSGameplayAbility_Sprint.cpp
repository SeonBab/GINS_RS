// Fill out your copyright notice in the Description page of Project Settings.


#include "RSGameplayAbility_Sprint.h"

#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "RSGameplayTags.h"

URSGameplayAbility_Sprint::URSGameplayAbility_Sprint()
{
	// 입력을 누르고 있는 동안 활성화를 시도하고, 입력 해제 이벤트를 받기 위해 인스턴스를 유지합니다
	ActivationPolicy = ERSAbilityActivationPolicy::WhileInputActive;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void URSGameplayAbility_Sprint::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	// 이 어빌리티는 CharacterMovementComponent의 MaxWalkSpeed를 직접 변경하므로 Character만 허용합니다
	ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
	if (!Character)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	UCharacterMovementComponent* MovementComponent = Character->GetCharacterMovement();
	if (!MovementComponent)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	// 비용과 쿨다운 등 Commit 조건을 만족하지 못하면 아무 상태도 적용하지 않고 취소합니다
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	// 종료 시 정확한 값으로 되돌릴 수 있도록 적용 직전의 이동 속도를 보관합니다
	CachedMovementComponent = MovementComponent;
	CachedMaxWalkSpeed = MovementComponent->MaxWalkSpeed;

	MovementComponent->MaxWalkSpeed = SprintSpeed;
	bSprintApplied = true;

	UAbilitySystemComponent* AbilitySystemComponent = ActorInfo->AbilitySystemComponent.Get();
	AbilitySystemComponent->AddLooseGameplayTag(RSGameplayTags::State_Movement_Sprinting);

	// 입력이 해제될 때까지 어빌리티를 유지하고 해제 이벤트에서 정상 종료합니다
	UAbilityTask_WaitInputRelease* WaitInputRelease = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
	WaitInputRelease->OnRelease.AddDynamic(this, &ThisClass::HandleInputReleased);
	WaitInputRelease->ReadyForActivation();
}

void URSGameplayAbility_Sprint::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (bSprintApplied)
	{
		// 정상 종료와 취소 모두 같은 경로를 사용하여 원래 이동 속도를 복구합니다
		if (UCharacterMovementComponent* MovementComponent = CachedMovementComponent.Get())
		{
			MovementComponent->MaxWalkSpeed = CachedMaxWalkSpeed;
		}

		if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
		{
			ActorInfo->AbilitySystemComponent->RemoveLooseGameplayTag(RSGameplayTags::State_Movement_Sprinting);
		}

		bSprintApplied = false;
		CachedMaxWalkSpeed = 0.0f;
		CachedMovementComponent.Reset();
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void URSGameplayAbility_Sprint::HandleInputReleased(float)
{
	// 입력 해제는 취소가 아닌 정상적인 스프린트 종료로 처리합니다
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
