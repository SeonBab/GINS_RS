// Fill out your copyright notice in the Description page of Project Settings.

#include "RSAbilityTask_ObserveAttackWindow.h"

#include "Abilities/GameplayAbility.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "RSAnimNotifyState_AttackWindow.h"
#include "RSAttackWindowMath.h"

URSAbilityTask_ObserveAttackWindow::URSAbilityTask_ObserveAttackWindow(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bTickingTask = true;
}

URSAbilityTask_ObserveAttackWindow* URSAbilityTask_ObserveAttackWindow::ObserveAttackWindow(UGameplayAbility* OwningAbility, UAnimMontage* Montage)
{
	URSAbilityTask_ObserveAttackWindow* Task = NewAbilityTask<URSAbilityTask_ObserveAttackWindow>(OwningAbility);
	Task->MontageToObserve = Montage;

	return Task;
}

bool URSAbilityTask_ObserveAttackWindow::TryGetAttackWindowRange(const UAnimMontage* Montage, float& OutWindowStartPosition, float& OutWindowEndPosition, int32* OutAttackWindowCount)
{
	OutWindowStartPosition = 0.0f;
	OutWindowEndPosition = 0.0f;

	int32 AttackWindowCount = 0;
	if (Montage)
	{
		for (const FAnimNotifyEvent& NotifyEvent : Montage->Notifies)
		{
			if (!NotifyEvent.NotifyStateClass || !NotifyEvent.NotifyStateClass->IsA<URSAnimNotifyState_AttackWindow>())
			{
				continue;
			}

			++AttackWindowCount;
			OutWindowStartPosition = NotifyEvent.GetTriggerTime();
			OutWindowEndPosition = NotifyEvent.GetEndTriggerTime();
		}
	}

	if (OutAttackWindowCount)
	{
		*OutAttackWindowCount = AttackWindowCount;
	}

	return AttackWindowCount == 1 && OutWindowEndPosition - OutWindowStartPosition > KINDA_SMALL_NUMBER;
}

void URSAbilityTask_ObserveAttackWindow::Activate()
{
	Super::Activate();

	const FGameplayAbilityActorInfo* ActorInfo = Ability ? Ability->GetCurrentActorInfo() : nullptr;
	UAnimInstance* AnimInstance = ActorInfo ? ActorInfo->GetAnimInstance() : nullptr;
	if (!TryGetAttackWindowRange(MontageToObserve, WindowStartPosition, WindowEndPosition) || !AnimInstance || !AnimInstance->Montage_IsActive(MontageToObserve))
	{
		InvalidateTask();

		return;
	}

	PreviousMontagePosition = AnimInstance->Montage_GetPosition(MontageToObserve);
	if (PreviousMontagePosition > WindowEndPosition + KINDA_SMALL_NUMBER)
	{
		InvalidateTask();

		return;
	}

	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnPositionUpdated.Broadcast(PreviousMontagePosition);
	}
	if (!ShouldBroadcastAbilityTaskDelegates())
	{
		return;
	}

	// Task가 같은 프레임에서 Window 내부에 시작되어도 시작부터 현재 위치까지의 판정을 보충합니다
	if (PreviousMontagePosition + KINDA_SMALL_NUMBER >= WindowStartPosition && !AdvanceWindow(PreviousMontagePosition))
	{
		InvalidateTask();
	}
}

void URSAbilityTask_ObserveAttackWindow::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);

	const FGameplayAbilityActorInfo* ActorInfo = Ability ? Ability->GetCurrentActorInfo() : nullptr;
	UAnimInstance* AnimInstance = ActorInfo ? ActorInfo->GetAnimInstance() : nullptr;
	if (!AnimInstance || !AnimInstance->Montage_IsActive(MontageToObserve))
	{
		InvalidateTask();

		return;
	}

	const float CurrentMontagePosition = AnimInstance->Montage_GetPosition(MontageToObserve);
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnPositionUpdated.Broadcast(CurrentMontagePosition);
	}
	if (!ShouldBroadcastAbilityTaskDelegates())
	{
		return;
	}

	if (!AdvanceWindow(CurrentMontagePosition))
	{
		InvalidateTask();
	}
}

bool URSAbilityTask_ObserveAttackWindow::AdvanceWindow(float CurrentMontagePosition)
{
	FRSAttackWindowStep WindowStep;
	if (!FRSAttackWindowMath::TryCalculateStep(PreviousMontagePosition, CurrentMontagePosition, WindowStartPosition, WindowEndPosition, bWindowActive, WindowStep))
	{
		return false;
	}

	if (WindowStep.bShouldBegin)
	{
		bWindowActive = true;
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnWindowBegan.Broadcast();
		}
	}

	if (WindowStep.bShouldAdvance && ShouldBroadcastAbilityTaskDelegates())
	{
		OnWindowAdvanced.Broadcast(WindowStep.PreviousAlpha, WindowStep.CurrentAlpha);
	}

	PreviousMontagePosition = CurrentMontagePosition;
	if (!WindowStep.bShouldEnd)
	{
		return true;
	}

	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnWindowEnded.Broadcast();
	}

	EndTask();

	return true;
}

void URSAbilityTask_ObserveAttackWindow::InvalidateTask()
{
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnInvalidated.Broadcast();
	}

	EndTask();
}
