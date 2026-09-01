// Fill out your copyright notice in the Description page of Project Settings.

#include "RSGameplayAbility_BasicAttack.h"

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "GameFramework/Character.h"
#include "RSGameplayTags.h"
#include "RSPlayerController.h"

URSGameplayAbility_BasicAttack::URSGameplayAbility_BasicAttack()
{
	ActivationPolicy = ERSAbilityActivationPolicy::WhileInputActive;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(RSGameplayTags::Ability_Combat_BasicAttack);
	SetAssetTags(AssetTags);

	// Montage의 행동 잠금 구간에서는 활성화를 막고 잠금이 끝난 뒤 새 공격이 진행 중인 대시를 교체하게 합니다
	ActivationBlockedTags.AddTag(RSGameplayTags::State_Action_Locked);
	CancelAbilitiesWithTag.AddTag(RSGameplayTags::Ability_Movement_Dash);
}

void URSGameplayAbility_BasicAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
	ARSPlayerController* PlayerController = Cast<ARSPlayerController>(ActorInfo->PlayerController.Get());
	if (!Character || !PlayerController || TemporaryAttackInterval <= 0.0f)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	FVector CursorWorldLocation;
	if (!PlayerController->GetCursorWorldLocation(CursorWorldLocation))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	FVector AttackDirection = CursorWorldLocation - Character->GetActorLocation();
	AttackDirection.Z = 0.0f;
	if (!AttackDirection.Normalize())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	// 공격 시작 이후 Navigation 경로 추종이 회전과 충돌하지 않게 현재 이동 요청을 중단합니다
	PlayerController->StopMovement();

	const FRotator AttackRotation = AttackDirection.Rotation();
	Character->SetActorRotation(FRotator(0.0, AttackRotation.Yaw, 0.0));

	// 현재 WaitDelay는 버튼 유지 반복만 먼저 검증하기 위한 임시 수명이며 추후 공격 Montage 수명으로 교체합니다
	UAbilityTask_WaitDelay* AttackIntervalTask = UAbilityTask_WaitDelay::WaitDelay(this, TemporaryAttackInterval);
	AttackIntervalTask->OnFinish.AddDynamic(this, &ThisClass::HandleTemporaryAttackIntervalFinished);
	AttackIntervalTask->ReadyForActivation();
}

void URSGameplayAbility_BasicAttack::HandleTemporaryAttackIntervalFinished()
{
	// Held 입력이 남아 있으면 Ability 종료 후 다음 입력 처리 프레임에서 새 공격이 활성화됩니다
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
