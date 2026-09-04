// Fill out your copyright notice in the Description page of Project Settings.

#include "RSGameplayAbility_Dash.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Tasks/RSAbilityTask_CurveMovement.h"
#include "RSGameplayTags.h"

URSGameplayAbility_Dash::URSGameplayAbility_Dash()
{
	ActivationPolicy = ERSAbilityActivationPolicy::OnInputTriggered;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(RSGameplayTags::Ability_Movement_Dash);
	SetAssetTags(AssetTags);

	// Montage의 행동 잠금 구간에서는 활성화를 막고 잠금이 끝난 뒤 새 대시가 진행 중인 기본 공격을 교체하게 합니다
	ActivationBlockedTags.AddTag(RSGameplayTags::State_Action_Locked);
	CancelAbilitiesWithTag.AddTag(RSGameplayTags::Ability_Combat_BasicAttack);

	ActivationOwnedTags.AddTag(RSGameplayTags::State_Movement_Dashing);
	CooldownTags.AddTag(RSGameplayTags::Cooldown_Ability_Dash);
	CooldownDuration = 1.0f;

	// 외부 애셋이 없어도 초반 이동이 빠르고 후반이 감속하는 개발용 곡선을 바로 검증할 수 있게 합니다
	FRichCurve* ProgressCurve = DistanceProgressCurve.GetRichCurve();
	ProgressCurve->AddKey(0.0f, 0.0f);
	ProgressCurve->AddKey(0.15f, 0.45f);
	ProgressCurve->AddKey(0.4f, 0.78f);
	ProgressCurve->AddKey(0.75f, 0.96f);
	ProgressCurve->AddKey(1.0f, 1.0f);
}

void URSGameplayAbility_Dash::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid())
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

	UCharacterMovementComponent* MovementComponent = Character->GetCharacterMovement();
	const FRichCurve* ProgressCurve = DistanceProgressCurve.GetRichCurveConst();
	if (!MovementComponent || !ProgressCurve || ProgressCurve->GetNumKeys() < 2 || DashDistance <= 0.0f || DashDuration <= 0.0f || DashMontagePlayRate <= 0.0f)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	const float ActiveDashDuration = DashMontage ? DashMontage->GetPlayLength() / DashMontagePlayRate : DashDuration;
	if (ActiveDashDuration <= 0.0f)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	const FVector DashDirection = GetCursorDirectionOrForward(ActorInfo, *Character);
	if (DashDirection.IsNearlyZero())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	// 모든 실행 조건을 확인한 뒤 Commit하여 실패한 대시가 쿨다운을 소비하지 않게 합니다
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	// Navigation 이동이 대시의 커브 이동과 동시에 캐릭터를 제어하지 않게 현재 경로를 중지합니다
	if (AController* Controller = Character->GetController())
	{
		Controller->StopMovement();
	}
	MovementComponent->StopMovementImmediately();

	const FRotator DashRotation = DashDirection.Rotation();
	Character->SetActorRotation(FRotator(0.0, DashRotation.Yaw, 0.0));

	ActiveDashMovementTask = URSAbilityTask_CurveMovement::CreateCurveMovement(this, MovementComponent, DashDirection, DashDistance, ActiveDashDuration, *ProgressCurve);
	ActiveDashMovementTask->ReadyForActivation();

	if (DashMontage)
	{
		UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, DashMontage, DashMontagePlayRate);
		MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleDashFinished);
		MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleDashCancelled);
		MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleDashCancelled);
		MontageTask->ReadyForActivation();

		return;
	}

	UAbilityTask_WaitDelay* DashDurationTask = UAbilityTask_WaitDelay::WaitDelay(this, ActiveDashDuration);
	DashDurationTask->OnFinish.AddDynamic(this, &ThisClass::HandleDashFinished);
	DashDurationTask->ReadyForActivation();
}

void URSGameplayAbility_Dash::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	EndAnimationGameplayStatesForMontage(ActorInfo, DashMontage);

	if (ActiveDashMovementTask && !ActiveDashMovementTask->IsFinished())
	{
		ActiveDashMovementTask->EndTask();
	}

	ActiveDashMovementTask = nullptr;

	// Commit에서 적용한 쿨다운은 대시가 취소되어도 원래 만료 시점까지 유지합니다
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void URSGameplayAbility_Dash::HandleDashFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void URSGameplayAbility_Dash::HandleDashCancelled()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}
