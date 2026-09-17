#include "RSGameplayAbility_PizzaMemoryPattern.h"

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "Combat/RSCircularSliceMath.h"
#include "RSAttackTelegraphComponent.h"
#include "RSGameplayTags.h"
#include "Tasks/RSAbilityTask_BossFacing.h"

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

void URSGameplayAbility_PizzaMemoryPattern::BeginPatternTimeline()
{
	LockedPatternTransform = FTransform::Identity;
	ActiveSliceShape = FRSCombatShape();
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

	const FRSBossFacingRequest FacingRequest = FRSBossFacingRequest::MakeFixedWorldYaw(WorldYawDegrees, FacingRotationSpeed, FacingYawTolerance, MaxFacingDuration);
	if (!CurrentActorInfo || !CurrentActorInfo->AvatarActor.IsValid() || !PizzaMemoryPatternDefinition.IsDataValid() || !HitDefinition.DamageEffectClass || !FacingRequest.IsDataValid())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	if (!CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	URSAbilityTask_BossFacing* FacingTask = CreateBossFacingTask(FacingRequest);
	if (!FacingTask)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	FacingTask->OnFacingFinished.AddDynamic(this, &ThisClass::HandleBossFacingFinished);
	FacingTask->ReadyForActivation();
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
	ActiveSliceShape = FRSCombatShape();
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

void URSGameplayAbility_PizzaMemoryPattern::HandleBossFacingFinished(ERSBossFacingResult Result, float FinalYawDegrees)
{
	if (bIsCleaningUp || !IsActive())
	{
		return;
	}

	if (Result != ERSBossFacingResult::Aligned && Result != ERSBossFacingResult::TimedOut)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	RequestAttackMontage();
	BeginAttackStartDelay();
}

void URSGameplayAbility_PizzaMemoryPattern::BeginAttackStartDelay()
{
	if (PizzaMemoryPatternDefinition.AttackStartDelay <= 0.0f)
	{
		HandleAttackStartDelayFinished();

		return;
	}

	AttackStartDelayTask = UAbilityTask_WaitDelay::WaitDelay(this, PizzaMemoryPatternDefinition.AttackStartDelay);
	AttackStartDelayTask->OnFinish.AddDynamic(this, &ThisClass::HandleAttackStartDelayFinished);
	AttackStartDelayTask->ReadyForActivation();
}

void URSGameplayAbility_PizzaMemoryPattern::HandleAttackStartDelayFinished()
{
	AttackStartDelayTask = nullptr;
	if (bIsCleaningUp || !IsActive())
	{
		return;
	}

	if (!TryCaptureLockedPatternState() || !TrySelectSafeZoneSequence() || !Timeline.Initialize(ActiveSafePairs.Num()))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	if (!BeginCurrentMemoryCue())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

bool URSGameplayAbility_PizzaMemoryPattern::TryCaptureLockedPatternState()
{
	const ACharacter* Character = CurrentActorInfo ? Cast<ACharacter>(CurrentActorInfo->AvatarActor.Get()) : nullptr;
	const UCapsuleComponent* CapsuleComp = Character ? Character->GetCapsuleComponent() : nullptr;
	if (!Character || !CapsuleComp)
	{
		return false;
	}

	if (!RSCircularSliceMath::TryCalculateLockedTransformFromCapsule(CapsuleComp->GetComponentTransform(), CapsuleComp->GetScaledCapsuleHalfHeight(), Character->GetActorForwardVector(), LockedPatternTransform))
	{
		return false;
	}

	// 예고, 판정과 연출이 이 한 번의 결과를 함께 읽으므로 세 경로의 형상이 갈라질 수 없습니다
	return PizzaMemoryPatternDefinition.TryMakeSliceShape(ActiveSliceShape);
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
	if (Timeline.GetPhase() != ERSPizzaCueTimelinePhase::Cue || !TelegraphComp || !ActiveSafePairs.IsValidIndex(SequenceIndex)
		|| !PizzaMemoryPatternDefinition.TryBuildDangerousSliceTransforms(LockedPatternTransform, ActiveSafePairs[SequenceIndex], ActiveDangerousSliceTransforms))
	{
		return false;
	}

	ActiveMemoryCueHandles.Reset();
	ActiveMemoryCueHandles.Reserve(ActiveDangerousSliceTransforms.Num());
	for (const FTransform& SliceTransform : ActiveDangerousSliceTransforms)
	{
		const int32 TelegraphHandle = TelegraphComp->ShowShapeWithExternalFill(ActiveSliceShape, SliceTransform);
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

	MemoryCueDurationTask = UAbilityTask_WaitDelay::WaitDelay(this, Timeline.GetCurrentDelaySeconds(PizzaMemoryPatternDefinition.MakeCueTimings()));
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

	if (Timeline.GetPhase() == ERSPizzaCueTimelinePhase::RecallDelay)
	{
		BeginRecallDelay();

		return;
	}

	MemoryCueGapTask = UAbilityTask_WaitDelay::WaitDelay(this, Timeline.GetCurrentDelaySeconds(PizzaMemoryPatternDefinition.MakeCueTimings()));
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
	if (Timeline.GetPhase() != ERSPizzaCueTimelinePhase::RecallDelay)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	const float RecallDelay = Timeline.GetCurrentDelaySeconds(PizzaMemoryPatternDefinition.MakeCueTimings());
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

	if (Timeline.GetPhase() == ERSPizzaCueTimelinePhase::Complete)
	{
		FinishPatternWhenMontageEnds();

		return;
	}

	ExplosionIntervalTask = UAbilityTask_WaitDelay::WaitDelay(this, Timeline.GetCurrentDelaySeconds(PizzaMemoryPatternDefinition.MakeCueTimings()));
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
	if (Timeline.GetPhase() != ERSPizzaCueTimelinePhase::Explosion || !AvatarActor || !ActiveSafePairs.IsValidIndex(SequenceIndex)
		|| !PizzaMemoryPatternDefinition.TryBuildDangerousSliceTransforms(LockedPatternTransform, ActiveSafePairs[SequenceIndex], ActiveDangerousSliceTransforms))
	{
		return false;
	}

	const float DamageAmount = GetPatternDamageAmount();
	if (!FMath::IsFinite(DamageAmount) || DamageAmount < 0.0f)
	{
		return false;
	}

	FRSCombatShape CandidateShape;
	CandidateShape.Type = ERSCombatShapeType::AnnularSector;
	CandidateShape.OuterRadius = PizzaMemoryPatternDefinition.OuterRadius;
	// 후보를 모으는 원반도 조각과 같은 안쪽 경계를 써야 보스 발밑이 판정에서 함께 빠집니다
	CandidateShape.InnerRadius = ActiveSliceShape.InnerRadius;

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
		ApplyPatternHitToTarget(CandidateTarget);
	}

	return true;
}

void URSGameplayAbility_PizzaMemoryPattern::PlayCurrentExplosionPresentation()
{
	PlayPatternPresentation(ActiveSliceShape, ActiveDangerousSliceTransforms, LockedPatternTransform.GetLocation());
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
		// 안쪽 경계를 에셋이 소유하므로 런타임과 같은 형상으로 검사합니다
		FRSCombatShape SliceShape;
		if (PizzaMemoryPatternDefinition.TryMakeSliceShape(SliceShape))
		{
			ValidationResult = CombineDataValidationResults(ValidationResult, ValidatePatternPresentation(SliceShape, Context));
		}
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

	const FRSBossFacingRequest FacingRequest = FRSBossFacingRequest::MakeFixedWorldYaw(WorldYawDegrees, FacingRotationSpeed, FacingYawTolerance, MaxFacingDuration);
	if (!FacingRequest.IsDataValid())
	{
		Context.AddError(FText::FromString(TEXT("FixedWorldYaw Facing values are outside their supported ranges.")));
		ValidationResult = EDataValidationResult::Invalid;
	}

	return ValidationResult;
}
#endif
