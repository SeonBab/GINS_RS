// Fill out your copyright notice in the Description page of Project Settings.

#include "RSGameplayAbility_Knockdown.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Combat/RSCombatFunctionLibrary.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "RSGameplayTags.h"
#include "Tasks/RSAbilityTask_CurveMovement.h"
#include "Tasks/RSAbilityTask_WaitMontageHeldAtSectionEnd.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace
{
	const FName StartSectionName(TEXT("Start"));
	const FName AirLoopSectionName(TEXT("AirLoop"));
	const FName FallSectionName(TEXT("Fall"));
	const FName LandSectionName(TEXT("Land"));

	struct FRSKnockdownMontageTiming
	{
		float StartDuration = 0.0f;
		float AirLoopDuration = 0.0f;
		float FallDuration = 0.0f;
		float LandDuration = 0.0f;
		float EffectiveAirborneDuration = 0.0f;
		int32 AirLoopCount = 0;
	};

	bool TryCalculateKnockdownMontageTiming(const UAnimMontage* Montage, float PlayRate, float RequestedDuration, FRSKnockdownMontageTiming& OutTiming)
	{
		OutTiming = FRSKnockdownMontageTiming();

		if (!Montage || !FMath::IsFinite(PlayRate) || PlayRate <= 0.0f || !FMath::IsFinite(RequestedDuration) || Montage->GetNumSections() != 4)
		{
			return false;
		}

		const TArray<FName> RequiredSections = { StartSectionName, AirLoopSectionName, FallSectionName, LandSectionName };
		for (int32 SectionIndex = 0; SectionIndex < RequiredSections.Num(); ++SectionIndex)
		{
			if (Montage->GetSectionName(SectionIndex) != RequiredSections[SectionIndex])
			{
				return false;
			}
		}

		OutTiming.StartDuration = Montage->GetSectionLength(0) / PlayRate;
		OutTiming.AirLoopDuration = Montage->GetSectionLength(1) / PlayRate;
		OutTiming.FallDuration = Montage->GetSectionLength(2) / PlayRate;
		OutTiming.LandDuration = Montage->GetSectionLength(3) / PlayRate;
		if (OutTiming.StartDuration <= 0.0f || OutTiming.AirLoopDuration <= 0.0f || OutTiming.FallDuration <= 0.0f || OutTiming.LandDuration <= 0.0f)
		{
			return false;
		}

		return URSGameplayAbility_Knockdown::TryCalculateAirborneTiming(RequestedDuration, OutTiming.StartDuration, OutTiming.AirLoopDuration, OutTiming.FallDuration, OutTiming.AirLoopCount, OutTiming.EffectiveAirborneDuration);
	}
}

URSGameplayAbility_Knockdown::URSGameplayAbility_Knockdown()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	ActivationOwnedTags.AddTag(RSGameplayTags::State_CrowdControl_Knockdown);

	// 넉백 면역이 요청을 거부하는 지점입니다
	ActivationBlockedTags.AddTag(RSGameplayTags::State_Immunity_Knockback);

	// 진행 중이던 행동과 더 약한 반응, 그리고 이미 누워 있던 상태를 모두 이 넉다운이 대체합니다
	CancelAbilitiesWithTag.AddTag(RSGameplayTags::Ability_Combat_BasicAttack);
	CancelAbilitiesWithTag.AddTag(RSGameplayTags::Ability_Movement_Dash);
	CancelAbilitiesWithTag.AddTag(RSGameplayTags::Ability_CrowdControl_HitReact);
	CancelAbilitiesWithTag.AddTag(RSGameplayTags::Ability_CrowdControl_Downed);

	FAbilityTriggerData TriggerData;
	TriggerData.TriggerTag = RSGameplayTags::GameplayEvent_CrowdControl_Knockdown;
	TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(TriggerData);

	// 맞은 직후 크게 밀리고 뒤로 갈수록 잦아드는 개발용 곡선입니다
	FRichCurve* ProgressCurve = DistanceProgressCurve.GetRichCurve();
	ProgressCurve->AddKey(0.0f, 0.0f);
	ProgressCurve->AddKey(0.2f, 0.55f);
	ProgressCurve->AddKey(0.5f, 0.85f);
	ProgressCurve->AddKey(1.0f, 1.0f);
}

bool URSGameplayAbility_Knockdown::TryCalculateAirborneTiming(float RequestedDuration, float StartDuration, float AirLoopDuration, float FallDuration, int32& OutAirLoopCount, float& OutEffectiveAirborneDuration)
{
	OutAirLoopCount = 0;
	OutEffectiveAirborneDuration = 0.0f;

	if (!FMath::IsFinite(RequestedDuration) || !FMath::IsFinite(StartDuration) || !FMath::IsFinite(AirLoopDuration) || !FMath::IsFinite(FallDuration)
		|| StartDuration <= 0.0f || AirLoopDuration <= 0.0f || FallDuration <= 0.0f)
	{
		return false;
	}

	const float FixedAirborneDuration = StartDuration + FallDuration;
	const float RemainingRequestedDuration = FMath::Max(RequestedDuration - FixedAirborneDuration, 0.0f);
	const float RequiredLoopCount = RemainingRequestedDuration / AirLoopDuration;
	OutAirLoopCount = FMath::Max(0, FMath::CeilToInt32(RequiredLoopCount - UE_KINDA_SMALL_NUMBER));
	OutEffectiveAirborneDuration = FixedAirborneDuration + AirLoopDuration * OutAirLoopCount;

	return FMath::IsFinite(OutEffectiveAirborneDuration) && OutEffectiveAirborneDuration > 0.0f;
}

void URSGameplayAbility_Knockdown::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	bReachedLand = false;
	bIsTransitioningToDowned = false;

	if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid() || !KnockdownMontage)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
	UCharacterMovementComponent* MovementComponent = Character ? Character->GetCharacterMovement() : nullptr;
	UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance();
	const FRichCurve* ProgressCurve = DistanceProgressCurve.GetRichCurveConst();
	if (!Character || !MovementComponent || !AnimInstance || !ProgressCurve || ProgressCurve->GetNumKeys() < 2)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	const FRSKnockbackTargetData* KnockbackData = nullptr;
	if (TriggerEventData && TriggerEventData->TargetData.Num() > 0)
	{
		const FGameplayAbilityTargetData* TargetData = TriggerEventData->TargetData.Get(0);
		if (TargetData && TargetData->GetScriptStruct() == FRSKnockbackTargetData::StaticStruct())
		{
			KnockbackData = static_cast<const FRSKnockbackTargetData*>(TargetData);
		}
	}

	FRSKnockdownMontageTiming MontageTiming;
	if (!KnockbackData || !FMath::IsFinite(KnockbackData->Distance) || !FMath::IsFinite(KnockbackData->Height) || !TryCalculateKnockdownMontageTiming(KnockdownMontage, KnockdownMontagePlayRate, KnockbackData->Duration, MontageTiming))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	// 넘어지는 동안 Navigation 경로 추종이 남아 캐릭터가 계속 움직이지 않게 현재 이동 요청을 중단합니다
	if (AController* Controller = Character->GetController())
	{
		Controller->StopMovement();
	}
	MovementComponent->StopMovementImmediately();

	FVector KnockbackDirection = FVector::ZeroVector;
	const bool bHasSpecifiedDirection = KnockbackData && KnockbackData->bHasKnockbackDirection;
	bool bHasValidDirection = bHasSpecifiedDirection && KnockbackData->TryGetNormalizedDirection(KnockbackDirection);
	if (!bHasSpecifiedDirection)
	{
		// 방향을 지정하지 않은 기존 공격만 공격자 반대 방향을 계속 사용합니다
		const AActor* Instigator = TriggerEventData ? TriggerEventData->Instigator.Get() : nullptr;
		KnockbackDirection = Instigator ? Character->GetActorLocation() - Instigator->GetActorLocation() : FVector::ZeroVector;
		KnockbackDirection.Z = 0.0f;
		bHasValidDirection = KnockbackDirection.Normalize();
	}

	if (bHasValidDirection)
	{
		// 이동 방향의 반대를 보게 하여 등 뒤쪽으로 날아가는 공용 애니메이션 기준을 유지합니다
		const FRotator FacingRotation = (-KnockbackDirection).Rotation();
		Character->SetActorRotation(FRotator(0.0, FacingRotation.Yaw, 0.0));
	}

	// 정상 Land handoff에서는 Ability 종료가 Montage를 먼저 정지하지 않게 하여 DefaultSlot의 기반 포즈가 노출되지 않도록 합니다
	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, KnockdownMontage, KnockdownMontagePlayRate, StartSectionName, false);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleKnockdownMontageCancelled);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleKnockdownMontageCancelled);
	MontageTask->ReadyForActivation();
	if (!IsActive())
	{
		return;
	}

	AnimInstance->Montage_SetNextSection(StartSectionName, MontageTiming.AirLoopCount > 0 ? AirLoopSectionName : FallSectionName, KnockdownMontage);
	AnimInstance->Montage_SetNextSection(AirLoopSectionName, MontageTiming.AirLoopCount > 1 ? AirLoopSectionName : FallSectionName, KnockdownMontage);
	AnimInstance->Montage_SetNextSection(FallSectionName, LandSectionName, KnockdownMontage);
	AnimInstance->Montage_SetNextSection(LandSectionName, NAME_None, KnockdownMontage);

	URSAbilityTask_WaitMontageHeldAtSectionEnd* LandCompletionTask = URSAbilityTask_WaitMontageHeldAtSectionEnd::WaitMontageHeldAtSectionEnd(this, KnockdownMontage, LandSectionName);
	LandCompletionTask->OnCompleted.AddDynamic(this, &ThisClass::HandleKnockdownLandCompleted);
	LandCompletionTask->OnInvalidated.AddDynamic(this, &ThisClass::HandleKnockdownMontageCancelled);
	LandCompletionTask->ReadyForActivation();
	if (!IsActive())
	{
		return;
	}

	if (MontageTiming.AirLoopCount > 1)
	{
		// 마지막 Loop 중간에 다음 Section만 Fall로 바꾸므로 현재 Loop의 끝 포즈까지 온전히 재생합니다
		const float PrepareFinalLoopDelay = MontageTiming.StartDuration + MontageTiming.AirLoopDuration * (static_cast<float>(MontageTiming.AirLoopCount) - 0.5f);
		UAbilityTask_WaitDelay* FinalLoopDelayTask = UAbilityTask_WaitDelay::WaitDelay(this, PrepareFinalLoopDelay);
		FinalLoopDelayTask->OnFinish.AddDynamic(this, &ThisClass::HandlePrepareFinalAirLoop);
		FinalLoopDelayTask->ReadyForActivation();
	}

	// 방향이 없으면 수평 이동만 생략하고 높이와 시간으로 만드는 제자리 아치는 유지합니다
	const float HorizontalDistance = bHasValidDirection ? FMath::Max(KnockbackData->Distance, 0.0f) : 0.0f;
	const float ArcHeight = FMath::Max3(KnockbackData->Height, MinimumKnockbackHeight, 0.0f);
	URSAbilityTask_CurveMovement* KnockbackTask = URSAbilityTask_CurveMovement::CreateCurveMovement(this, MovementComponent, KnockbackDirection, HorizontalDistance, MontageTiming.EffectiveAirborneDuration, *ProgressCurve);
	KnockbackTask->SetMeshArc(Character->GetMesh(), ArcHeight);
	KnockbackTask->OnCompleted.AddDynamic(this, &ThisClass::HandleKnockbackMovementCompleted);
	KnockbackTask->ReadyForActivation();
}

void URSGameplayAbility_Knockdown::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	EndAnimationGameplayStatesForMontage(ActorInfo, KnockdownMontage);

	bReachedLand = false;
	bIsTransitioningToDowned = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void URSGameplayAbility_Knockdown::HandlePrepareFinalAirLoop()
{
	UAnimInstance* AnimInstance = CurrentActorInfo ? CurrentActorInfo->GetAnimInstance() : nullptr;
	if (!AnimInstance || !AnimInstance->Montage_IsPlaying(KnockdownMontage))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	AnimInstance->Montage_SetNextSection(AirLoopSectionName, FallSectionName, KnockdownMontage);
}

void URSGameplayAbility_Knockdown::HandleKnockbackMovementCompleted()
{
	UAnimInstance* AnimInstance = CurrentActorInfo ? CurrentActorInfo->GetAnimInstance() : nullptr;
	if (!AnimInstance || !AnimInstance->Montage_IsPlaying(KnockdownMontage))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	bReachedLand = true;
	if (AnimInstance->Montage_GetCurrentSection(KnockdownMontage) != LandSectionName)
	{
		// 이동 Task의 끝을 지면 접촉의 Source of Truth로 삼아 같은 프레임에 Land를 시작합니다
		AnimInstance->Montage_JumpToSection(LandSectionName, KnockdownMontage);
	}
}

void URSGameplayAbility_Knockdown::HandleKnockdownLandCompleted()
{
	if (!IsActive() || !bReachedLand || bIsTransitioningToDowned)
	{
		return;
	}

	UAbilitySystemComponent* AbilitySystemComponent = CurrentActorInfo ? CurrentActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!AbilitySystemComponent)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	bIsTransitioningToDowned = true;
	const bool bActivationRequested = TryActivateAbilityByClass(AbilitySystemComponent, DownedAbilityClass);
	FGameplayAbilitySpec* DownedAbilitySpec = AbilitySystemComponent->FindAbilitySpecFromClass(DownedAbilityClass);
	if (!bActivationRequested || !DownedAbilitySpec || !DownedAbilitySpec->IsActive())
	{
		bIsTransitioningToDowned = false;
		UE_LOG(LogTemp, Warning, TEXT("%s could not activate %s after knockdown"), *GetName(), *GetNameSafe(DownedAbilityClass));

		if (UAnimInstance* AnimInstance = CurrentActorInfo ? CurrentActorInfo->GetAnimInstance() : nullptr)
		{
			AnimInstance->Montage_Stop(0.0f, KnockdownMontage);
		}

		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	ensureMsgf(AbilitySystemComponent->HasMatchingGameplayTag(RSGameplayTags::State_CrowdControl_Downed), TEXT("%s is active without State.CrowdControl.Downed"), *GetNameSafe(DownedAbilityClass));
	if (UGameplayAbility* DownedAbilityInstance = DownedAbilitySpec->GetPrimaryInstance())
	{
		ensureMsgf(AbilitySystemComponent->IsAnimatingAbility(DownedAbilityInstance), TEXT("%s is active without animation ownership"), *GetNameSafe(DownedAbilityClass));
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void URSGameplayAbility_Knockdown::HandleKnockdownMontageCancelled()
{
	if (!IsActive() || bIsTransitioningToDowned)
	{
		return;
	}

	// bStopWhenAbilityEnds=false이므로 정상 handoff가 아닌 종료에서는 현재 Montage를 명시적으로 정리합니다
	if (UAnimInstance* AnimInstance = CurrentActorInfo ? CurrentActorInfo->GetAnimInstance() : nullptr)
	{
		AnimInstance->Montage_Stop(0.0f, KnockdownMontage);
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

#if WITH_EDITOR
EDataValidationResult URSGameplayAbility_Knockdown::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult ValidationResult = Super::IsDataValid(Context);

	FRSKnockdownMontageTiming MontageTiming;
	if (!TryCalculateKnockdownMontageTiming(KnockdownMontage, KnockdownMontagePlayRate, 0.0f, MontageTiming))
	{
		Context.AddError(FText::FromString(TEXT("KnockdownMontage must contain exactly Start, AirLoop, Fall, and Land Sections in that order, with positive lengths and a valid PlayRate.")));
		ValidationResult = EDataValidationResult::Invalid;
	}
	else if (KnockdownMontage->bEnableAutoBlendOut)
	{
		Context.AddError(FText::FromString(TEXT("KnockdownMontage must disable Auto Blend Out so the Land pose remains until Downed starts.")));
		ValidationResult = EDataValidationResult::Invalid;
	}

	if (!DownedAbilityClass)
	{
		Context.AddError(FText::FromString(TEXT("DownedAbilityClass is required.")));
		ValidationResult = EDataValidationResult::Invalid;
	}

	if (!FMath::IsFinite(MinimumKnockbackHeight) || MinimumKnockbackHeight < 0.0f)
	{
		Context.AddError(FText::FromString(TEXT("MinimumKnockbackHeight must be finite and non-negative.")));
		ValidationResult = EDataValidationResult::Invalid;
	}

	const FRichCurve* ProgressCurve = DistanceProgressCurve.GetRichCurveConst();
	if (!ProgressCurve || ProgressCurve->GetNumKeys() < 2)
	{
		Context.AddError(FText::FromString(TEXT("DistanceProgressCurve requires at least two keys.")));
		ValidationResult = EDataValidationResult::Invalid;
	}

	return ValidationResult;
}
#endif
