// Fill out your copyright notice in the Description page of Project Settings.


#include "RSGameplayAbility_BossGroggy.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace
{
	const FName GroggyStartSectionName(TEXT("Start"));
	const FName GroggyLoopSectionName(TEXT("Loop"));
	const FName GroggyEndSectionName(TEXT("End"));

	bool TryGetGroggySectionTiming(const UAnimMontage* Montage, float PlayRate, float RequestedDuration, float& OutStartDuration, float& OutLoopDuration, float& OutEndDuration, int32& OutLoopCount, float& OutEffectiveDuration)
	{
		if (!Montage || !FMath::IsFinite(PlayRate) || PlayRate <= 0.0f || Montage->GetNumSections() != 3
			|| Montage->GetSectionName(0) != GroggyStartSectionName || Montage->GetSectionName(1) != GroggyLoopSectionName || Montage->GetSectionName(2) != GroggyEndSectionName)
		{
			return false;
		}

		OutStartDuration = Montage->GetSectionLength(0) / PlayRate;
		OutLoopDuration = Montage->GetSectionLength(1) / PlayRate;
		OutEndDuration = Montage->GetSectionLength(2) / PlayRate;

		return URSGameplayAbility_BossGroggy::TryCalculateGroggyTiming(RequestedDuration, OutStartDuration, OutLoopDuration, OutEndDuration, OutLoopCount, OutEffectiveDuration);
	}
}

URSGameplayAbility_BossGroggy::URSGameplayAbility_BossGroggy()
{
	// Montage 종료를 기다리는 동안 Task 참조를 유지해야 하므로 Actor별 인스턴스를 사용합니다
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

bool URSGameplayAbility_BossGroggy::TryCalculateGroggyTiming(float RequestedDuration, float StartDuration, float LoopDuration, float EndDuration, int32& OutLoopCount, float& OutEffectiveDuration)
{
	OutLoopCount = 0;
	OutEffectiveDuration = 0.0f;

	if (!FMath::IsFinite(RequestedDuration) || !FMath::IsFinite(StartDuration) || !FMath::IsFinite(LoopDuration) || !FMath::IsFinite(EndDuration)
		|| RequestedDuration <= 0.0f || StartDuration <= 0.0f || LoopDuration <= 0.0f || EndDuration <= 0.0f)
	{
		return false;
	}

	const float AvailableLoopDuration = RequestedDuration - StartDuration - EndDuration;
	OutLoopCount = FMath::FloorToInt32((AvailableLoopDuration + UE_KINDA_SMALL_NUMBER) / LoopDuration);
	if (OutLoopCount < 1)
	{
		OutLoopCount = 0;

		return false;
	}

	OutEffectiveDuration = StartDuration + LoopDuration * OutLoopCount + EndDuration;

	return FMath::IsFinite(OutEffectiveDuration) && OutEffectiveDuration <= RequestedDuration + UE_KINDA_SMALL_NUMBER;
}

void URSGameplayAbility_BossGroggy::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	bIsEndingAbility = false;
	float StartDuration = 0.0f;
	float LoopDuration = 0.0f;
	float EndDuration = 0.0f;
	float EffectiveDuration = 0.0f;
	int32 LoopCount = 0;
	if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid() || !TryGetGroggySectionTiming(GroggyMontage, GroggyMontagePlayRate, GroggyDuration, StartDuration, LoopDuration, EndDuration, LoopCount, EffectiveDuration))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, GroggyMontage, GroggyMontagePlayRate, GroggyStartSectionName);
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleGroggyMontageFinished);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleGroggyMontageCancelled);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleGroggyMontageCancelled);
	MontageTask->ReadyForActivation();
	if (!IsActive())
	{
		return;
	}

	UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance();
	if (!AnimInstance || !AnimInstance->Montage_IsPlaying(GroggyMontage))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	AnimInstance->Montage_SetNextSection(GroggyStartSectionName, GroggyLoopSectionName, GroggyMontage);
	AnimInstance->Montage_SetNextSection(GroggyLoopSectionName, LoopCount > 1 ? GroggyLoopSectionName : GroggyEndSectionName, GroggyMontage);
	AnimInstance->Montage_SetNextSection(GroggyEndSectionName, NAME_None, GroggyMontage);

	if (LoopCount > 1)
	{
		const float PrepareFinalLoopDelay = StartDuration + LoopDuration * (static_cast<float>(LoopCount) - 0.5f);
		UAbilityTask_WaitDelay* FinalLoopDelayTask = UAbilityTask_WaitDelay::WaitDelay(this, PrepareFinalLoopDelay);
		FinalLoopDelayTask->OnFinish.AddDynamic(this, &ThisClass::HandlePrepareFinalLoop);
		FinalLoopDelayTask->ReadyForActivation();
	}
}

void URSGameplayAbility_BossGroggy::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (bIsEndingAbility)
	{
		return;
	}

	bIsEndingAbility = true;
	EndAnimationGameplayStatesForMontage(ActorInfo, GroggyMontage);

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	bIsEndingAbility = false;
}

void URSGameplayAbility_BossGroggy::HandleGroggyMontageFinished()
{
	if (!bIsEndingAbility && IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void URSGameplayAbility_BossGroggy::HandleGroggyMontageCancelled()
{
	if (!bIsEndingAbility && IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

void URSGameplayAbility_BossGroggy::HandlePrepareFinalLoop()
{
	UAnimInstance* AnimInstance = CurrentActorInfo ? CurrentActorInfo->GetAnimInstance() : nullptr;
	if (!AnimInstance || !AnimInstance->Montage_IsPlaying(GroggyMontage))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	AnimInstance->Montage_SetNextSection(GroggyLoopSectionName, GroggyEndSectionName, GroggyMontage);
}

#if WITH_EDITOR
EDataValidationResult URSGameplayAbility_BossGroggy::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult ValidationResult = Super::IsDataValid(Context);
	float StartDuration = 0.0f;
	float LoopDuration = 0.0f;
	float EndDuration = 0.0f;
	float EffectiveDuration = 0.0f;
	int32 LoopCount = 0;
	if (!TryGetGroggySectionTiming(GroggyMontage, GroggyMontagePlayRate, GroggyDuration, StartDuration, LoopDuration, EndDuration, LoopCount, EffectiveDuration))
	{
		Context.AddError(FText::FromString(TEXT("GroggyMontage must contain exactly Start, Loop, and End Sections in that order, use a positive play rate, and fit at least one complete Loop inside GroggyDuration.")));
		ValidationResult = EDataValidationResult::Invalid;
	}
	else if (!GroggyMontage->bEnableAutoBlendOut)
	{
		Context.AddError(FText::FromString(TEXT("GroggyMontage must enable Auto Blend Out so the ability receives normal completion after End.")));
		ValidationResult = EDataValidationResult::Invalid;
	}

	return ValidationResult;
}
#endif
