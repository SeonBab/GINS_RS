// Fill out your copyright notice in the Description page of Project Settings.

#include "RSGameplayAbility_Knockdown.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "Combat/RSCombatFunctionLibrary.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "RSAbilitySystemComponent.h"
#include "RSGameplayTags.h"
#include "Tasks/RSAbilityTask_CurveMovement.h"

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

void URSGameplayAbility_Knockdown::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	bTransitionToDowned = false;

	if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid() || !KnockdownMontage)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
	UCharacterMovementComponent* MovementComponent = Character ? Character->GetCharacterMovement() : nullptr;
	const FRichCurve* ProgressCurve = DistanceProgressCurve.GetRichCurveConst();
	if (!Character || !MovementComponent || !ProgressCurve || ProgressCurve->GetNumKeys() < 2)
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

	// 공격자가 없거나 같은 위치라면 밀려나지 않고 그 자리에서 넘어집니다
	const AActor* Instigator = TriggerEventData ? TriggerEventData->Instigator.Get() : nullptr;
	FVector KnockbackDirection = Instigator ? Character->GetActorLocation() - Instigator->GetActorLocation() : FVector::ZeroVector;
	KnockbackDirection.Z = 0.0f;

	if (KnockbackDirection.Normalize())
	{
		// 밀려나는 방향의 반대를 보게 하여 공격자를 향한 채 넘어지게 합니다
		const FRotator FacingRotation = (-KnockbackDirection).Rotation();
		Character->SetActorRotation(FRotator(0.0, FacingRotation.Yaw, 0.0));
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

	if (KnockbackData && !KnockbackDirection.IsNearlyZero() && KnockbackData->Distance > 0.0f && KnockbackData->Duration > 0.0f)
	{
		ActiveKnockbackTask = URSAbilityTask_CurveMovement::CreateCurveMovement(this, MovementComponent, KnockbackDirection, KnockbackData->Distance, KnockbackData->Duration, *ProgressCurve);
		ActiveKnockbackTask->SetMeshArc(Character->GetMesh(), KnockbackData->Height);
		ActiveKnockbackTask->ReadyForActivation();
	}

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, KnockdownMontage);
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleKnockdownMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleKnockdownMontageCancelled);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleKnockdownMontageCancelled);
	MontageTask->ReadyForActivation();
}

void URSGameplayAbility_Knockdown::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (KnockdownMontage && ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
	{
		if (URSAbilitySystemComponent* AbilitySystemComponent = Cast<URSAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get()))
		{
			// Montage 중단으로 Notify End를 받지 못한 경우에만 남아 있는 상태를 출처별로 정리합니다
			AbilitySystemComponent->EndAnimationGameplayStates(KnockdownMontage);
		}
	}

	if (ActiveKnockbackTask && !ActiveKnockbackTask->IsFinished())
	{
		ActiveKnockbackTask->EndTask();
	}

	ActiveKnockbackTask = nullptr;

	const bool bShouldEnterDowned = bTransitionToDowned && !bWasCancelled;
	bTransitionToDowned = false;

	// EndAbility 이후에는 현재 ActorInfo를 신뢰할 수 없으므로 전이에 사용할 ASC를 미리 확보합니다
	UAbilitySystemComponent* AbilitySystemComponent = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;

	// 넘어짐 상태 태그가 먼저 제거되어야 누움과 두 상태가 겹치지 않습니다
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);

	if (!bShouldEnterDowned)
	{
		return;
	}

	// 누움으로 넘기지 못하면 아무 상태도 남기지 않고 정상 행동으로 돌아갑니다
	if (!TryActivateAbilityByClass(AbilitySystemComponent, DownedAbilityClass))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s could not activate %s after knockdown"), *GetName(), *GetNameSafe(DownedAbilityClass));
	}
}

void URSGameplayAbility_Knockdown::HandleKnockdownMontageCompleted()
{
	bTransitionToDowned = true;

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void URSGameplayAbility_Knockdown::HandleKnockdownMontageCancelled()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}
