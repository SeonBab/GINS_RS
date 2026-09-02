// Fill out your copyright notice in the Description page of Project Settings.

#include "RSGameplayAbility_BasicAttack.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Character.h"
#include "GameplayEffect.h"
#include "RSAbilitySystemComponent.h"
#include "RSGameplayTags.h"
#include "RSPlayerController.h"

URSGameplayAbility_BasicAttack::URSGameplayAbility_BasicAttack()
{
	ActivationPolicy = ERSAbilityActivationPolicy::OnInputTriggered;
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
	if (!Character || !PlayerController || !AttackMontage)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	if (NextComboReadyTag.IsValid() && (!ComboStateEffectClass || ComboReadyDuration <= 0.0f))
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

	ConsumeRequiredComboState();

	// 공격 시작 이후 Navigation 경로 추종이 회전과 충돌하지 않게 현재 이동 요청을 중단합니다
	PlayerController->StopMovement();

	const FRotator AttackRotation = AttackDirection.Rotation();
	Character->SetActorRotation(FRotator(0.0, AttackRotation.Yaw, 0.0));

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, AttackMontage);
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleAttackMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleAttackMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleAttackMontageCancelled);
	MontageTask->ReadyForActivation();
}

void URSGameplayAbility_BasicAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (AttackMontage && ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
	{
		if (URSAbilitySystemComponent* AbilitySystemComponent = Cast<URSAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get()))
		{
			// Montage 중단으로 Notify End를 받지 못한 경우에만 남아 있는 상태를 출처별로 정리합니다
			AbilitySystemComponent->EndAnimationGameplayStates(AttackMontage);
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void URSGameplayAbility_BasicAttack::HandleAttackMontageCompleted()
{
	ApplyNextComboState();
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void URSGameplayAbility_BasicAttack::HandleAttackMontageInterrupted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void URSGameplayAbility_BasicAttack::HandleAttackMontageCancelled()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

FGameplayTag URSGameplayAbility_BasicAttack::GetRequiredComboReadyTag() const
{
	FGameplayTag RequiredComboReadyTag;

	for (const FGameplayTag& RequiredTag : ActivationRequiredTags)
	{
		if (!RequiredTag.MatchesTag(RSGameplayTags::State_Combo_BasicAttack_Ready) || RequiredTag.MatchesTagExact(RSGameplayTags::State_Combo_BasicAttack_Ready))
		{
			continue;
		}

		// 한 단계가 여러 콤보 준비 상태를 요구하면 어떤 상태를 소비할지 모호하므로 구성 오류를 즉시 드러냅니다
		if (!ensureMsgf(!RequiredComboReadyTag.IsValid(), TEXT("%s의 ActivationRequiredTags에는 콤보 준비 단계 태그를 하나만 설정해야 합니다"), *GetName()))
		{
			return FGameplayTag();
		}

		RequiredComboReadyTag = RequiredTag;
	}

	return RequiredComboReadyTag;
}

void URSGameplayAbility_BasicAttack::ConsumeRequiredComboState()
{
	const FGameplayTag RequiredComboReadyTag = GetRequiredComboReadyTag();
	if (!RequiredComboReadyTag.IsValid() || !CurrentActorInfo || !CurrentActorInfo->AbilitySystemComponent.IsValid())
	{
		return;
	}

	FGameplayTagContainer TagsToRemove;
	TagsToRemove.AddTag(RequiredComboReadyTag);
	CurrentActorInfo->AbilitySystemComponent->RemoveActiveEffectsWithGrantedTags(TagsToRemove);
}

void URSGameplayAbility_BasicAttack::ApplyNextComboState()
{
	if (!NextComboReadyTag.IsValid())
	{
		return;
	}

	if (!CurrentActorInfo || !CurrentActorInfo->AbilitySystemComponent.IsValid() || !ComboStateEffectClass || ComboReadyDuration <= 0.0f)
	{
		return;
	}

	const float AbilityLevel = GetAbilityLevel(CurrentSpecHandle, CurrentActorInfo);
	FGameplayEffectSpecHandle ComboStateSpecHandle = MakeOutgoingGameplayEffectSpec(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, ComboStateEffectClass, AbilityLevel);
	if (!ComboStateSpecHandle.IsValid())
	{
		return;
	}

	// 공용 GameplayEffect에 단계를 고정하지 않고 현재 공격이 여는 다음 단계와 시간을 실행별로 설정합니다
	ComboStateSpecHandle.Data->DynamicGrantedTags.AddTag(NextComboReadyTag);
	ComboStateSpecHandle.Data->SetSetByCallerMagnitude(RSGameplayTags::SetByCaller_Combo_Duration, ComboReadyDuration);
	ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, ComboStateSpecHandle);
}
