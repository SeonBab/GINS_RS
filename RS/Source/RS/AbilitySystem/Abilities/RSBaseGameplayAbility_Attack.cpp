// Fill out your copyright notice in the Description page of Project Settings.

#include "RSBaseGameplayAbility_Attack.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemGlobals.h"
#include "Animation/AnimMontage.h"
#include "Combat/RSCombatFunctionLibrary.h"
#include "GameplayEffect.h"
#include "RSAbilitySystemComponent.h"
#include "RSGameplayTags.h"
#include "RSHealthSet.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#include "RSAnimNotify_GameplayEvent.h"
#endif

URSBaseGameplayAbility_Attack::URSBaseGameplayAbility_Attack()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// Montage의 행동 잠금 구간에서는 새 공격이 시작되지 않게 합니다
	ActivationBlockedTags.AddTag(RSGameplayTags::State_Action_Locked);
}

void URSBaseGameplayAbility_Attack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid() || !ActorInfo->AvatarActor.IsValid() || !AttackMontage)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	// 실행 조건을 모두 확인한 뒤 Commit하여 실패한 공격이 쿨다운을 소비하지 않게 합니다
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	// 방향 결정은 조종 주체마다 다르므로 기반에서는 현재 전방을 그대로 사용합니다
	StartAttackMontage();
}

void URSBaseGameplayAbility_Attack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
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

#if WITH_EDITOR
EDataValidationResult URSBaseGameplayAbility_Attack::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult ValidationResult = Super::IsDataValid(Context);

	if (!AttackMontage)
	{
		return ValidationResult;
	}

	// 판정 Notify는 몇 번째 타격인지 구분하지 않고 순서로만 대응하므로 개수가 맞지 않으면 조용히 어긋납니다
	int32 HitCheckNotifyCount = 0;
	for (const FAnimNotifyEvent& NotifyEvent : AttackMontage->Notifies)
	{
		const URSAnimNotify_GameplayEvent* GameplayEventNotify = Cast<URSAnimNotify_GameplayEvent>(NotifyEvent.Notify);
		if (GameplayEventNotify && GameplayEventNotify->GetEventTag().MatchesTagExact(RSGameplayTags::GameplayEvent_Combat_HitCheck))
		{
			++HitCheckNotifyCount;
		}
	}

	if (HitCheckNotifyCount != HitChecks.Num())
	{
		Context.AddError(FText::FromString(FString::Printf(TEXT("%s의 판정 Notify는 %d개인데 Hit Checks는 %d개입니다"), *GetNameSafe(AttackMontage), HitCheckNotifyCount, HitChecks.Num())));
		ValidationResult = EDataValidationResult::Invalid;
	}

	return ValidationResult;
}
#endif

void URSBaseGameplayAbility_Attack::StartAttackMontage()
{
	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, AttackMontage);
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleAttackMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleAttackMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleAttackMontageCancelled);
	MontageTask->ReadyForActivation();

	// InstancedPerActor라 인스턴스가 재사용되므로 이번 활성화가 첫 타격부터 시작하도록 되돌립니다
	NextHitCheckIndex = 0;

	// 한 Montage에 판정 시점이 여러 개일 수 있으므로 첫 이벤트에서 대기를 끝내지 않습니다
	UAbilityTask_WaitGameplayEvent* HitCheckTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, RSGameplayTags::GameplayEvent_Combat_HitCheck, nullptr, false);
	HitCheckTask->EventReceived.AddDynamic(this, &ThisClass::HandleHitCheckEvent);
	HitCheckTask->ReadyForActivation();
}

void URSBaseGameplayAbility_Attack::HandleAttackMontageCompleted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void URSBaseGameplayAbility_Attack::HandleAttackMontageInterrupted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void URSBaseGameplayAbility_Attack::HandleAttackMontageCancelled()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void URSBaseGameplayAbility_Attack::HandleHitCheckEvent(FGameplayEventData Payload)
{
	if (!HitChecks.IsValidIndex(NextHitCheckIndex))
	{
		// 설정이 어긋난 상태이므로 판정 디버그 여부와 상관없이 항상 알립니다
		UE_LOG(LogTemp, Warning, TEXT("%s received hit check %d but only %d hit checks are configured"), *GetName(), NextHitCheckIndex, HitChecks.Num());

		return;
	}

	const int32 HitCheckIndex = NextHitCheckIndex;
	const FRSHitCheckDefinition& HitCheck = HitChecks[HitCheckIndex];
	++NextHitCheckIndex;

	AActor* AvatarActor = CurrentActorInfo ? CurrentActorInfo->AvatarActor.Get() : nullptr;
	if (!AvatarActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s has no avatar actor for hit check %d"), *GetName(), HitCheckIndex);

		return;
	}

	// 배치 규칙을 데이터로 분리하기 전까지 이 어빌리티는 공격자 전방 배치 하나만 사용합니다
	FRSCombatShape HitCheckShape;
	HitCheckShape.BoxExtent = HitCheck.BoxExtent;

	const FVector HitCheckLocation = AvatarActor->GetActorLocation() + AvatarActor->GetActorForwardVector() * HitCheck.ForwardOffset;
	const FTransform HitCheckTransform(AvatarActor->GetActorQuat(), HitCheckLocation);

	TArray<AActor*> HitTargets;
	URSCombatFunctionLibrary::FindTargetsInShape(AvatarActor, TargetChannel, HitCheckShape, HitCheckTransform, HitTargets);

	if (HitTargets.IsEmpty())
	{
		if (URSCombatFunctionLibrary::IsHitCheckDebugEnabled())
		{
			UE_LOG(LogTemp, Log, TEXT("%s hit check %d found no target"), *GetName(), HitCheckIndex);
		}

		return;
	}

	const float DamageAmount = HitCheck.Damage.GetValueAtLevel(GetAbilityLevel(CurrentSpecHandle, CurrentActorInfo));
	for (AActor* HitTarget : HitTargets)
	{
		ApplyDamageToTarget(HitTarget, DamageAmount);
		SendHitReactionToTarget(HitTarget, HitCheck);
	}
}

void URSBaseGameplayAbility_Attack::ApplyDamageToTarget(AActor* TargetActor, float DamageAmount)
{
	if (!TargetActor || !DamageEffectClass || !CurrentActorInfo || !CurrentActorInfo->AbilitySystemComponent.IsValid())
	{
		return;
	}

	UAbilitySystemComponent* TargetAbilitySystemComp = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor);
	if (!TargetAbilitySystemComp)
	{
		return;
	}

	const float AbilityLevel = GetAbilityLevel(CurrentSpecHandle, CurrentActorInfo);
	FGameplayEffectSpecHandle DamageSpecHandle = MakeOutgoingGameplayEffectSpec(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, DamageEffectClass, AbilityLevel);
	if (!DamageSpecHandle.IsValid())
	{
		return;
	}

	// 공용 대미지 GameplayEffect에 피해량을 고정하지 않고 이번 타격의 값을 실행별로 설정합니다
	DamageSpecHandle.Data->SetSetByCallerMagnitude(RSGameplayTags::SetByCaller_Damage, DamageAmount);
	CurrentActorInfo->AbilitySystemComponent->ApplyGameplayEffectSpecToTarget(*DamageSpecHandle.Data.Get(), TargetAbilitySystemComp);

	if (URSCombatFunctionLibrary::IsHitCheckDebugEnabled())
	{
		const URSHealthSet* TargetHealthSet = TargetAbilitySystemComp->GetSet<URSHealthSet>();
		UE_LOG(LogTemp, Log, TEXT("%s applied %.1f damage to %s, remaining health %.1f"), *GetName(), DamageAmount, *GetNameSafe(TargetActor), TargetHealthSet ? TargetHealthSet->GetHealth() : -1.0f);
	}
}

void URSBaseGameplayAbility_Attack::SendHitReactionToTarget(AActor* TargetActor, const FRSHitCheckDefinition& HitCheck) const
{
	if (!TargetActor || HitCheck.Reaction == ERSHitReactionType::None)
	{
		return;
	}

	AActor* AvatarActor = CurrentActorInfo ? CurrentActorInfo->AvatarActor.Get() : nullptr;

	FGameplayEventData ReactionPayload;
	ReactionPayload.Instigator = AvatarActor;
	ReactionPayload.Target = TargetActor;

	switch (HitCheck.Reaction)
	{
	case ERSHitReactionType::HitReact:
		ReactionPayload.EventTag = RSGameplayTags::GameplayEvent_CrowdControl_HitReact;

		break;

	case ERSHitReactionType::Knockdown:
	{
		ReactionPayload.EventTag = RSGameplayTags::GameplayEvent_CrowdControl_Knockdown;

		// 넉다운은 거리와 높이, 시간이 타격마다 다르므로 float 하나인 EventMagnitude 대신 TargetData로 전달합니다
		FRSKnockbackTargetData* KnockbackData = new FRSKnockbackTargetData();
		KnockbackData->Distance = HitCheck.KnockbackDistance;
		KnockbackData->Height = HitCheck.KnockbackHeight;
		KnockbackData->Duration = HitCheck.KnockbackDuration;
		ReactionPayload.TargetData.Add(KnockbackData);

		break;
	}

	default:
		return;
	}

	// 면역 판정은 대상의 반응 Ability가 하므로 여기서는 요청만 보냅니다
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(TargetActor, ReactionPayload.EventTag, ReactionPayload);
}
