// Fill out your copyright notice in the Description page of Project Settings.

#include "RSAbilityTask_WaitMontageHeldAtSectionEnd.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Abilities/GameplayAbility.h"

URSAbilityTask_WaitMontageHeldAtSectionEnd::URSAbilityTask_WaitMontageHeldAtSectionEnd(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bTickingTask = true;
}

URSAbilityTask_WaitMontageHeldAtSectionEnd* URSAbilityTask_WaitMontageHeldAtSectionEnd::WaitMontageHeldAtSectionEnd(UGameplayAbility* OwningAbility, UAnimMontage* Montage, FName SectionName)
{
	URSAbilityTask_WaitMontageHeldAtSectionEnd* Task = NewAbilityTask<URSAbilityTask_WaitMontageHeldAtSectionEnd>(OwningAbility);
	Task->MontageToObserve = Montage;
	Task->ExpectedSectionName = SectionName;

	return Task;
}

void URSAbilityTask_WaitMontageHeldAtSectionEnd::Activate()
{
	Super::Activate();

	if (!Ability || !MontageToObserve || ExpectedSectionName.IsNone() || MontageToObserve->GetSectionIndex(ExpectedSectionName) == INDEX_NONE || MontageToObserve->bEnableAutoBlendOut)
	{
		InvalidateTask();
	}
}

void URSAbilityTask_WaitMontageHeldAtSectionEnd::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);

	const FGameplayAbilityActorInfo* ActorInfo = Ability ? Ability->GetCurrentActorInfo() : nullptr;
	UAnimInstance* AnimInstance = ActorInfo ? ActorInfo->GetAnimInstance() : nullptr;
	if (!AnimInstance || !AnimInstance->Montage_IsActive(MontageToObserve))
	{
		InvalidateTask();

		return;
	}

	if (AnimInstance->Montage_IsPlaying(MontageToObserve))
	{
		return;
	}

	if (AnimInstance->Montage_GetCurrentSection(MontageToObserve) != ExpectedSectionName)
	{
		InvalidateTask();

		return;
	}

	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnCompleted.Broadcast();
	}

	EndTask();
}

void URSAbilityTask_WaitMontageHeldAtSectionEnd::InvalidateTask()
{
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnInvalidated.Broadcast();
	}

	EndTask();
}
