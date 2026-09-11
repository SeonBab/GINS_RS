// Fill out your copyright notice in the Description page of Project Settings.

#include "RSGameplayAbility_BasicAttack.h"

#include "AbilitySystemComponent.h"
#include "GameFramework/Character.h"
#include "GameplayEffect.h"
#include "RSGameplayTags.h"
#include "RSPlayerController.h"

URSGameplayAbility_BasicAttack::URSGameplayAbility_BasicAttack()
{
	ActivationPolicy = ERSAbilityActivationPolicy::OnInputTriggered;

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(RSGameplayTags::Ability_Combat_BasicAttack);
	SetAssetTags(AssetTags);

	// 다른 행동은 그대로 두고 기본 공격만 막아야 하는 구간을 위한 차단입니다
	// 기반 클래스가 검사하는 State.Action.Locked와 형제 태그라 두 잠금은 서로 영향을 주지 않습니다
	ActivationBlockedTags.AddTag(RSGameplayTags::State_Action_BasicAttackLocked);

	// 잠금이 끝난 뒤 새 공격이 진행 중인 대시를 교체하게 합니다
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

	const FVector AttackDirection = GetCursorDirectionOrForward(ActorInfo, *Character);
	if (AttackDirection.IsNearlyZero())
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
	if (!ApplyNextComboState())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	// 공격 시작 이후 Navigation 경로 추종이 회전과 충돌하지 않게 현재 이동 요청을 중단합니다
	PlayerController->StopMovement();

	const FRotator AttackRotation = AttackDirection.Rotation();
	Character->SetActorRotation(FRotator(0.0, AttackRotation.Yaw, 0.0));

	StartAttackMontage();
}

void URSGameplayAbility_BasicAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (bWasCancelled && NextComboStateEffectHandle.IsValid() && ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
	{
		ActorInfo->AbilitySystemComponent->RemoveActiveGameplayEffect(NextComboStateEffectHandle);
	}

	NextComboStateEffectHandle.Invalidate();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
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

bool URSGameplayAbility_BasicAttack::ApplyNextComboState()
{
	if (!NextComboReadyTag.IsValid())
	{
		return true;
	}

	if (!CurrentActorInfo || !CurrentActorInfo->AbilitySystemComponent.IsValid() || !AttackMontage || !ComboStateEffectClass || ComboReadyDuration <= 0.0f)
	{
		return false;
	}

	const float AbilityLevel = GetAbilityLevel(CurrentSpecHandle, CurrentActorInfo);
	FGameplayEffectSpecHandle ComboStateSpecHandle = MakeOutgoingGameplayEffectSpec(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, ComboStateEffectClass, AbilityLevel);
	if (!ComboStateSpecHandle.IsValid())
	{
		return false;
	}

	// 다음 단계는 현재 공격 중에도 선택할 수 있어야 하므로 Montage 수명과 완료 후 유예 시간을 함께 보장합니다
	const float ComboStateDuration = AttackMontage->GetPlayLength() + ComboReadyDuration;
	ComboStateSpecHandle.Data->DynamicGrantedTags.AddTag(NextComboReadyTag);
	ComboStateSpecHandle.Data->SetSetByCallerMagnitude(RSGameplayTags::SetByCaller_Combo_Duration, ComboStateDuration);
	NextComboStateEffectHandle = ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, ComboStateSpecHandle);

	return NextComboStateEffectHandle.IsValid();
}

UAnimMontage* URSGameplayAbility_BasicAttack::GetAttackMontage() const
{
	return AttackMontage;
}
