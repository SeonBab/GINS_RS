#include "RSGameplayAbility_PizzaMemoryPattern.h"

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "Combat/RSCircularSliceMath.h"
#include "RSAttackTelegraphComponent.h"
#include "RSGameplayTags.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

URSGameplayAbility_PizzaMemoryPattern::URSGameplayAbility_PizzaMemoryPattern()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	ActivationBlockedTags.AddTag(RSGameplayTags::State_Action_Locked);

	// 위험한 여섯 조각의 기준점이 아니라 각 조각의 전체 면적에 폭발 연출을 배치합니다
	FRSBossPatternNiagaraEntry& FillNiagaraEntry = PatternPresentation.Niagaras.AddDefaulted_GetRef();
	FillNiagaraEntry.Placement = ERSBossPatternNiagaraPlacement::FillHitShape;
}

void URSGameplayAbility_PizzaMemoryPattern::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	LockedPatternTransform = FTransform::Identity;
	ActiveSafePairs.Reset();
	ActiveDangerousSliceTransforms.Reset();
	ActiveMemoryCueHandles.Reset();
	Timeline.Reset();
	HitActors.Reset();
	AttackStartDelayTask = nullptr;
	MemoryCueDurationTask = nullptr;
	MemoryCueGapTask = nullptr;
	RecallDelayTask = nullptr;
	ExplosionIntervalTask = nullptr;
	bIsCleaningUp = false;
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid() || !PizzaMemoryPatternDefinition.IsDataValid() || !DamageEffectClass)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	RequestAttackMontage();

	if (PizzaMemoryPatternDefinition.AttackStartDelay <= 0.0f)
	{
		HandleAttackStartDelayFinished();

		return;
	}

	AttackStartDelayTask = UAbilityTask_WaitDelay::WaitDelay(this, PizzaMemoryPatternDefinition.AttackStartDelay);
	AttackStartDelayTask->OnFinish.AddDynamic(this, &ThisClass::HandleAttackStartDelayFinished);
	AttackStartDelayTask->ReadyForActivation();
}

void URSGameplayAbility_PizzaMemoryPattern::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (bIsCleaningUp)
	{
		return;
	}

	bIsCleaningUp = true;
	if (AttackStartDelayTask)
	{
		AttackStartDelayTask->EndTask();
		AttackStartDelayTask = nullptr;
	}
	if (MemoryCueDurationTask)
	{
		MemoryCueDurationTask->EndTask();
		MemoryCueDurationTask = nullptr;
	}
	if (MemoryCueGapTask)
	{
		MemoryCueGapTask->EndTask();
		MemoryCueGapTask = nullptr;
	}
	if (RecallDelayTask)
	{
		RecallDelayTask->EndTask();
		RecallDelayTask = nullptr;
	}
	if (ExplosionIntervalTask)
	{
		ExplosionIntervalTask->EndTask();
		ExplosionIntervalTask = nullptr;
	}

	HideActiveMemoryCue();
	LockedPatternTransform = FTransform::Identity;
	ActiveSafePairs.Reset();
	ActiveDangerousSliceTransforms.Reset();
	Timeline.Reset();
	HitActors.Reset();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	bIsCleaningUp = false;
}

void URSGameplayAbility_PizzaMemoryPattern::RequestAttackMontage()
{
	PlayPatternMontage(AttackMontage);
}

void URSGameplayAbility_PizzaMemoryPattern::HandleAttackStartDelayFinished()
{
	AttackStartDelayTask = nullptr;
	if (bIsCleaningUp || !IsActive())
	{
		return;
	}

	if (!TryCaptureLockedPatternTransform() || !TrySelectSafeZoneSequence() || !Timeline.Initialize(ActiveSafePairs.Num()))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	if (!BeginCurrentMemoryCue())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

bool URSGameplayAbility_PizzaMemoryPattern::TryCaptureLockedPatternTransform()
{
	const ACharacter* Character = CurrentActorInfo ? Cast<ACharacter>(CurrentActorInfo->AvatarActor.Get()) : nullptr;
	const UCapsuleComponent* CapsuleComp = Character ? Character->GetCapsuleComponent() : nullptr;
	if (!Character || !CapsuleComp)
	{
		return false;
	}

	return RSCircularSliceMath::TryCalculateLockedTransformFromCapsule(CapsuleComp->GetComponentTransform(), CapsuleComp->GetScaledCapsuleHalfHeight(), Character->GetActorForwardVector(), LockedPatternTransform);
}

bool URSGameplayAbility_PizzaMemoryPattern::TrySelectSafeZoneSequence()
{
	const int32 CandidateCount = PizzaMemoryPatternDefinition.SafeZoneSequenceCandidates.Num();
	if (CandidateCount <= 0)
	{
		ActiveSafePairs.Reset();

		return false;
	}

	const int32 CandidateIndex = FMath::RandHelper(CandidateCount);

	return PizzaMemoryPatternDefinition.TryCopySafeZoneSequenceCandidate(CandidateIndex, ActiveSafePairs);
}

bool URSGameplayAbility_PizzaMemoryPattern::BeginCurrentMemoryCue()
{
	const int32 SequenceIndex = Timeline.GetSequenceIndex();
	AActor* AvatarActor = CurrentActorInfo ? CurrentActorInfo->AvatarActor.Get() : nullptr;
	URSAttackTelegraphComponent* TelegraphComp = AvatarActor ? AvatarActor->FindComponentByClass<URSAttackTelegraphComponent>() : nullptr;
	if (Timeline.GetPhase() != ERSPizzaMemoryPatternTimelinePhase::MemoryCue || !TelegraphComp || !ActiveSafePairs.IsValidIndex(SequenceIndex)
		|| !PizzaMemoryPatternDefinition.TryBuildDangerousSliceTransforms(LockedPatternTransform, ActiveSafePairs[SequenceIndex], ActiveDangerousSliceTransforms))
	{
		return false;
	}

	FRSCombatShape SliceShape;
	SliceShape.Type = ERSCombatShapeType::Cone;
	SliceShape.Range = PizzaMemoryPatternDefinition.OuterRadius;
	SliceShape.Angle = PizzaMemoryPatternDefinition.CalculateSliceAngleDegrees();

	ActiveMemoryCueHandles.Reset();
	ActiveMemoryCueHandles.Reserve(ActiveDangerousSliceTransforms.Num());
	for (const FTransform& SliceTransform : ActiveDangerousSliceTransforms)
	{
		const int32 TelegraphHandle = TelegraphComp->ShowShapeWithExternalFill(SliceShape, SliceTransform);
		if (TelegraphHandle == INDEX_NONE || !TelegraphComp->SetExternalFill(TelegraphHandle, 1.0f))
		{
			if (TelegraphHandle != INDEX_NONE)
			{
				TelegraphComp->HideShape(TelegraphHandle);
			}
			HideActiveMemoryCue();

			return false;
		}

		ActiveMemoryCueHandles.Add(TelegraphHandle);
	}

	MemoryCueDurationTask = UAbilityTask_WaitDelay::WaitDelay(this, Timeline.GetCurrentDelaySeconds(PizzaMemoryPatternDefinition));
	MemoryCueDurationTask->OnFinish.AddDynamic(this, &ThisClass::HandleMemoryCueDurationFinished);
	MemoryCueDurationTask->ReadyForActivation();

	return true;
}

void URSGameplayAbility_PizzaMemoryPattern::HandleMemoryCueDurationFinished()
{
	MemoryCueDurationTask = nullptr;
	if (bIsCleaningUp || !IsActive())
	{
		return;
	}

	HideActiveMemoryCue();
	ActiveDangerousSliceTransforms.Reset();

	if (!Timeline.Advance())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	if (Timeline.GetPhase() == ERSPizzaMemoryPatternTimelinePhase::RecallDelay)
	{
		BeginRecallDelay();

		return;
	}

	MemoryCueGapTask = UAbilityTask_WaitDelay::WaitDelay(this, Timeline.GetCurrentDelaySeconds(PizzaMemoryPatternDefinition));
	MemoryCueGapTask->OnFinish.AddDynamic(this, &ThisClass::HandleMemoryCueGapFinished);
	MemoryCueGapTask->ReadyForActivation();
}

void URSGameplayAbility_PizzaMemoryPattern::HandleMemoryCueGapFinished()
{
	MemoryCueGapTask = nullptr;
	if (bIsCleaningUp || !IsActive())
	{
		return;
	}

	if (!Timeline.Advance() || !BeginCurrentMemoryCue())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

void URSGameplayAbility_PizzaMemoryPattern::BeginRecallDelay()
{
	if (Timeline.GetPhase() != ERSPizzaMemoryPatternTimelinePhase::RecallDelay)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	const float RecallDelay = Timeline.GetCurrentDelaySeconds(PizzaMemoryPatternDefinition);
	if (RecallDelay <= 0.0f)
	{
		HandleRecallDelayFinished();

		return;
	}

	RecallDelayTask = UAbilityTask_WaitDelay::WaitDelay(this, RecallDelay);
	RecallDelayTask->OnFinish.AddDynamic(this, &ThisClass::HandleRecallDelayFinished);
	RecallDelayTask->ReadyForActivation();
}

void URSGameplayAbility_PizzaMemoryPattern::HandleRecallDelayFinished()
{
	RecallDelayTask = nullptr;
	if (bIsCleaningUp || !IsActive())
	{
		return;
	}

	if (!Timeline.Advance())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	RunCurrentExplosion();
}

void URSGameplayAbility_PizzaMemoryPattern::RunCurrentExplosion()
{
	if (!ExecuteCurrentExplosion())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	PlayCurrentExplosionPresentation();
	ActiveDangerousSliceTransforms.Reset();

	if (!Timeline.Advance())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	if (Timeline.GetPhase() == ERSPizzaMemoryPatternTimelinePhase::Complete)
	{
		FinishPatternWhenMontageEnds();

		return;
	}

	ExplosionIntervalTask = UAbilityTask_WaitDelay::WaitDelay(this, Timeline.GetCurrentDelaySeconds(PizzaMemoryPatternDefinition));
	ExplosionIntervalTask->OnFinish.AddDynamic(this, &ThisClass::HandleExplosionIntervalFinished);
	ExplosionIntervalTask->ReadyForActivation();
}

void URSGameplayAbility_PizzaMemoryPattern::HandleExplosionIntervalFinished()
{
	ExplosionIntervalTask = nullptr;
	if (bIsCleaningUp || !IsActive())
	{
		return;
	}

	if (!Timeline.Advance())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	RunCurrentExplosion();
}

bool URSGameplayAbility_PizzaMemoryPattern::ExecuteCurrentExplosion()
{
	const int32 SequenceIndex = Timeline.GetSequenceIndex();
	AActor* AvatarActor = CurrentActorInfo ? CurrentActorInfo->AvatarActor.Get() : nullptr;
	if (Timeline.GetPhase() != ERSPizzaMemoryPatternTimelinePhase::Explosion || !AvatarActor || !ActiveSafePairs.IsValidIndex(SequenceIndex)
		|| !PizzaMemoryPatternDefinition.TryBuildDangerousSliceTransforms(LockedPatternTransform, ActiveSafePairs[SequenceIndex], ActiveDangerousSliceTransforms))
	{
		return false;
	}

	const float DamageAmount = PizzaMemoryPatternDefinition.Damage.GetValueAtLevel(GetAbilityLevel(CurrentSpecHandle, CurrentActorInfo));
	if (!FMath::IsFinite(DamageAmount) || DamageAmount < 0.0f)
	{
		return false;
	}

	FRSCombatShape CandidateShape;
	CandidateShape.Type = ERSCombatShapeType::Sphere;
	CandidateShape.Radius = PizzaMemoryPatternDefinition.OuterRadius;
	CandidateShape.InnerRadius = 0.0f;

	TArray<AActor*> CandidateTargets;
	URSCombatFunctionLibrary::FindTargetsInShapeWithoutDebugDraw(AvatarActor, TargetChannel, CandidateShape, LockedPatternTransform, CandidateTargets);

	HitActors.Reset();
	for (AActor* CandidateTarget : CandidateTargets)
	{
		const TWeakObjectPtr<AActor> TargetPointer(CandidateTarget);
		bool bIsSafe = false;
		if (!CandidateTarget || HitActors.Contains(TargetPointer)
			|| !PizzaMemoryPatternDefinition.TryIsLocationInSafePair(LockedPatternTransform, ActiveSafePairs[SequenceIndex], CandidateTarget->GetActorLocation(), bIsSafe)
			|| bIsSafe)
		{
			continue;
		}

		HitActors.Add(TargetPointer);
		ApplyDamageToTarget(CandidateTarget, DamageEffectClass, DamageAmount);
		URSCombatFunctionLibrary::SendHitReaction(AvatarActor, CandidateTarget, PizzaMemoryPatternDefinition.Reaction);
	}

	return true;
}

void URSGameplayAbility_PizzaMemoryPattern::PlayCurrentExplosionPresentation()
{
	FRSCombatShape SliceShape;
	SliceShape.Type = ERSCombatShapeType::Cone;
	SliceShape.Range = PizzaMemoryPatternDefinition.OuterRadius;
	SliceShape.Angle = PizzaMemoryPatternDefinition.CalculateSliceAngleDegrees();

	PlayPatternPresentation(SliceShape, ActiveDangerousSliceTransforms, LockedPatternTransform.GetLocation());
}

void URSGameplayAbility_PizzaMemoryPattern::HideActiveMemoryCue()
{
	AActor* AvatarActor = CurrentActorInfo ? CurrentActorInfo->AvatarActor.Get() : nullptr;
	URSAttackTelegraphComponent* TelegraphComp = AvatarActor ? AvatarActor->FindComponentByClass<URSAttackTelegraphComponent>() : nullptr;
	if (TelegraphComp)
	{
		for (int32 TelegraphHandle : ActiveMemoryCueHandles)
		{
			TelegraphComp->HideShape(TelegraphHandle);
		}
	}

	ActiveMemoryCueHandles.Reset();
}

#if WITH_EDITOR
EDataValidationResult URSGameplayAbility_PizzaMemoryPattern::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult ValidationResult = Super::IsDataValid(Context);

	FString ValidationError;
	if (!PizzaMemoryPatternDefinition.IsDataValid(&ValidationError))
	{
		Context.AddError(FText::FromString(FString::Printf(TEXT("PizzaMemoryPatternDefinition is invalid: %s"), *ValidationError)));
		ValidationResult = EDataValidationResult::Invalid;
	}
	else
	{
		// 여섯 조각은 같은 반지름과 각도를 쓰므로 조각 하나로 격자 배치 설정을 대표해 검사합니다
		FRSCombatShape SliceShape;
		SliceShape.Type = ERSCombatShapeType::Cone;
		SliceShape.Range = PizzaMemoryPatternDefinition.OuterRadius;
		SliceShape.Angle = PizzaMemoryPatternDefinition.CalculateSliceAngleDegrees();

		ValidationResult = CombineDataValidationResults(ValidationResult, ValidatePatternPresentation(SliceShape, Context));
	}

	for (int32 EntryIndex = 0; EntryIndex < PatternPresentation.Niagaras.Num(); ++EntryIndex)
	{
		const FRSBossPatternNiagaraEntry& NiagaraEntry = PatternPresentation.Niagaras[EntryIndex];
		if (NiagaraEntry.Niagara.NiagaraSystem && NiagaraEntry.Placement != ERSBossPatternNiagaraPlacement::FillHitShape)
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("PatternPresentation.Niagaras[%d] must use FillHitShape so the effect covers every dangerous slice."), EntryIndex)));
			ValidationResult = EDataValidationResult::Invalid;
		}
	}

	if (!DamageEffectClass)
	{
		Context.AddError(FText::FromString(TEXT("DamageEffectClass is required.")));
		ValidationResult = EDataValidationResult::Invalid;
	}

	return ValidationResult;
}
#endif
