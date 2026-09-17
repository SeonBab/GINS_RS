#include "RSGameplayAbility_PizzaPattern.h"

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

namespace
{
	constexpr int32 PizzaPatternGroupCount = 2;
}

bool FRSPizzaPatternDefinition::IsDataValid(FString* OutValidationError) const
{
	if (OutValidationError)
	{
		OutValidationError->Reset();
	}

	auto SetValidationError = [OutValidationError](const TCHAR* ErrorMessage)
		{
			if (OutValidationError)
			{
				*OutValidationError = ErrorMessage;
			}
		};

	if (SliceCount < 2)
	{
		SetValidationError(TEXT("SliceCount must be at least 2."));

		return false;
	}

	if (ExplosionCount < 1)
	{
		SetValidationError(TEXT("ExplosionCount must be at least 1."));

		return false;
	}

	if (!FMath::IsFinite(OuterRadius) || OuterRadius <= 0.0f)
	{
		SetValidationError(TEXT("OuterRadius must be finite and greater than zero."));

		return false;
	}

	if (!FMath::IsFinite(InnerRadius) || InnerRadius < 0.0f || InnerRadius >= OuterRadius)
	{
		SetValidationError(TEXT("InnerRadius must be finite and satisfy 0 <= InnerRadius < OuterRadius."));

		return false;
	}

	if (!FMath::IsFinite(AttackStartDelay) || AttackStartDelay < 0.0f)
	{
		SetValidationError(TEXT("AttackStartDelay must be finite and non-negative."));

		return false;
	}

	if (!FMath::IsFinite(TelegraphCueDuration) || TelegraphCueDuration <= 0.0f)
	{
		SetValidationError(TEXT("TelegraphCueDuration must be finite and greater than zero."));

		return false;
	}

	if (!FMath::IsFinite(RecallDelay) || RecallDelay < 0.0f)
	{
		SetValidationError(TEXT("RecallDelay must be finite and non-negative."));

		return false;
	}

	// 폭발이 하나뿐이면 예고 사이와 폭발 사이가 존재하지 않으므로 값을 요구하지 않습니다
	if (ExplosionCount >= PizzaPatternGroupCount)
	{
		if (!FMath::IsFinite(TelegraphCueGap) || TelegraphCueGap <= 0.0f)
		{
			SetValidationError(TEXT("TelegraphCueGap must be finite and greater than zero so consecutive cues remain distinguishable."));

			return false;
		}

		if (!FMath::IsFinite(ExplosionInterval) || ExplosionInterval <= 0.0f)
		{
			SetValidationError(TEXT("ExplosionInterval must be finite and greater than zero."));

			return false;
		}
	}

	return true;
}

int32 FRSPizzaPatternDefinition::CalculateVirtualSliceCount() const
{
	return SliceCount >= 2 && SliceCount <= MAX_int32 / PizzaPatternGroupCount ? SliceCount * PizzaPatternGroupCount : 0;
}

float FRSPizzaPatternDefinition::CalculateSliceAngleDegrees() const
{
	return RSCircularSliceMath::CalculateSliceAngleDegrees(CalculateVirtualSliceCount());
}

bool FRSPizzaPatternDefinition::TryMakeSliceShape(FRSCombatShape& OutSliceShape) const
{
	OutSliceShape = FRSCombatShape();

	// 조각 Transform이 조각 중심을 향하므로 첫 경계를 조각 각도의 절반만큼 뒤로 물립니다
	const float SliceAngleDegrees = CalculateSliceAngleDegrees();
	OutSliceShape.Type = ERSCombatShapeType::AnnularSector;
	OutSliceShape.InnerRadius = InnerRadius;
	OutSliceShape.OuterRadius = OuterRadius;
	OutSliceShape.StartYawOffset = -SliceAngleDegrees * 0.5f;
	OutSliceShape.SweepAngleDegrees = SliceAngleDegrees;

	return OutSliceShape.IsDataValid();
}

FRSPizzaCueTimings FRSPizzaPatternDefinition::MakeCueTimings() const
{
	FRSPizzaCueTimings CueTimings;
	CueTimings.CueDuration = TelegraphCueDuration;
	CueTimings.CueGap = TelegraphCueGap;
	CueTimings.RecallDelay = RecallDelay;
	CueTimings.ExplosionInterval = ExplosionInterval;

	return CueTimings;
}

bool FRSPizzaPatternDefinition::TryBuildExplosionSliceTransforms(const FTransform& LockedTransform, int32 ExplosionIndex, TArray<FTransform>& OutSliceTransforms) const
{
	OutSliceTransforms.Reset();
	const int32 VirtualSliceCount = CalculateVirtualSliceCount();
	if (VirtualSliceCount <= 0 || ExplosionIndex < 0)
	{
		return false;
	}

	const int32 GroupParity = ExplosionIndex % PizzaPatternGroupCount;
	OutSliceTransforms.Reserve(SliceCount);
	for (int32 GroupSliceIndex = 0; GroupSliceIndex < SliceCount; ++GroupSliceIndex)
	{
		const int32 VirtualSliceIndex = GroupSliceIndex * PizzaPatternGroupCount + GroupParity;
		FTransform SliceTransform;
		if (!RSCircularSliceMath::TryBuildSliceTransform(LockedTransform, VirtualSliceCount, VirtualSliceIndex, SliceTransform))
		{
			OutSliceTransforms.Reset();

			return false;
		}

		OutSliceTransforms.Add(SliceTransform);
	}

	return true;
}

bool FRSPizzaPatternDefinition::IsLocationInExplosionGroup(const FTransform& LockedTransform, int32 ExplosionIndex, const FVector& TargetLocation) const
{
	const int32 VirtualSliceCount = CalculateVirtualSliceCount();
	if (VirtualSliceCount <= 0 || ExplosionIndex < 0)
	{
		return false;
	}

	int32 VirtualSliceIndex = INDEX_NONE;
	return RSCircularSliceMath::TryCalculateSliceIndex(LockedTransform, VirtualSliceCount, TargetLocation, VirtualSliceIndex)
		&& VirtualSliceIndex % PizzaPatternGroupCount == ExplosionIndex % PizzaPatternGroupCount;
}

URSGameplayAbility_PizzaPattern::URSGameplayAbility_PizzaPattern()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	ActivationBlockedTags.AddTag(RSGameplayTags::State_Action_Locked);

	// 이번에 터지는 조각과 안전한 조각을 구분해서 보여 줘야 하므로 조각 중심 한 점 대신 조각을 채웁니다
	FRSBossPatternNiagaraEntry& FillNiagaraEntry = PatternPresentation.Niagaras.AddDefaulted_GetRef();
	FillNiagaraEntry.Placement = ERSBossPatternNiagaraPlacement::FillHitShape;
}

void URSGameplayAbility_PizzaPattern::BeginPatternTimeline()
{
	LockedPatternTransform = FTransform::Identity;
	ActiveSliceShape = FRSCombatShape();
	ActiveSliceTransforms.Reset();
	ActiveTelegraphHandles.Reset();
	Timeline.Reset();
	HitActors.Reset();
	AttackStartDelayTask = nullptr;
	TelegraphCueDurationTask = nullptr;
	TelegraphCueGapTask = nullptr;
	RecallDelayTask = nullptr;
	ExplosionIntervalTask = nullptr;
	bIsCleaningUp = false;

	if (!CurrentActorInfo || !CurrentActorInfo->AvatarActor.IsValid() || !PizzaPatternDefinition.IsDataValid() || !HitDefinition.DamageEffectClass)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	if (!CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	RequestAttackMontage();

	if (PizzaPatternDefinition.AttackStartDelay <= 0.0f)
	{
		HandleAttackStartDelayFinished();

		return;
	}

	AttackStartDelayTask = UAbilityTask_WaitDelay::WaitDelay(this, PizzaPatternDefinition.AttackStartDelay);
	AttackStartDelayTask->OnFinish.AddDynamic(this, &ThisClass::HandleAttackStartDelayFinished);
	AttackStartDelayTask->ReadyForActivation();
}

void URSGameplayAbility_PizzaPattern::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
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
	if (TelegraphCueDurationTask)
	{
		TelegraphCueDurationTask->EndTask();
		TelegraphCueDurationTask = nullptr;
	}
	if (TelegraphCueGapTask)
	{
		TelegraphCueGapTask->EndTask();
		TelegraphCueGapTask = nullptr;
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

	HideActiveTelegraphs();

	LockedPatternTransform = FTransform::Identity;
	ActiveSliceShape = FRSCombatShape();
	ActiveSliceTransforms.Reset();
	Timeline.Reset();
	HitActors.Reset();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	bIsCleaningUp = false;
}

void URSGameplayAbility_PizzaPattern::RequestAttackMontage()
{
	// 재생 실패와 중단은 패턴을 취소하지 않고 기다릴 대상만 없앱니다
	// 공통 게이트가 Montage 수명을 알고 있으므로 마지막 폭발 뒤에도 재생 중인 Montage가 잘리지 않습니다
	PlayPatternMontage(AttackMontage);
}

void URSGameplayAbility_PizzaPattern::HandleAttackStartDelayFinished()
{
	AttackStartDelayTask = nullptr;
	if (bIsCleaningUp || !IsActive())
	{
		return;
	}

	if (!TryCaptureLockedPatternState() || !Timeline.Initialize(PizzaPatternDefinition.ExplosionCount) || !BeginCurrentTelegraphCue())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

bool URSGameplayAbility_PizzaPattern::TryCaptureLockedPatternState()
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
	return PizzaPatternDefinition.TryMakeSliceShape(ActiveSliceShape);
}

bool URSGameplayAbility_PizzaPattern::BeginCurrentTelegraphCue()
{
	AActor* AvatarActor = CurrentActorInfo ? CurrentActorInfo->AvatarActor.Get() : nullptr;
	URSAttackTelegraphComponent* TelegraphComp = AvatarActor ? AvatarActor->FindComponentByClass<URSAttackTelegraphComponent>() : nullptr;
	if (Timeline.GetPhase() != ERSPizzaCueTimelinePhase::Cue || !TelegraphComp
		|| !PizzaPatternDefinition.TryBuildExplosionSliceTransforms(LockedPatternTransform, Timeline.GetSequenceIndex(), ActiveSliceTransforms))
	{
		return false;
	}

	// 폭발 순서는 예고 순서로만 읽히므로 각 그룹은 채우는 과정 없이 완성된 상태로 한 번에 보여 줍니다
	ActiveTelegraphHandles.Reset();
	ActiveTelegraphHandles.Reserve(ActiveSliceTransforms.Num());
	for (const FTransform& SliceTransform : ActiveSliceTransforms)
	{
		const int32 TelegraphHandle = TelegraphComp->ShowShapeWithExternalFill(ActiveSliceShape, SliceTransform);
		if (TelegraphHandle == INDEX_NONE || !TelegraphComp->SetExternalFill(TelegraphHandle, 1.0f))
		{
			if (TelegraphHandle != INDEX_NONE)
			{
				TelegraphComp->HideShape(TelegraphHandle);
			}
			HideActiveTelegraphs();

			return false;
		}

		ActiveTelegraphHandles.Add(TelegraphHandle);
	}

	TelegraphCueDurationTask = UAbilityTask_WaitDelay::WaitDelay(this, Timeline.GetCurrentDelaySeconds(PizzaPatternDefinition.MakeCueTimings()));
	TelegraphCueDurationTask->OnFinish.AddDynamic(this, &ThisClass::HandleTelegraphCueDurationFinished);
	TelegraphCueDurationTask->ReadyForActivation();

	return true;
}

void URSGameplayAbility_PizzaPattern::HandleTelegraphCueDurationFinished()
{
	TelegraphCueDurationTask = nullptr;
	if (bIsCleaningUp || !IsActive())
	{
		return;
	}

	HideActiveTelegraphs();
	ActiveSliceTransforms.Reset();

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

	TelegraphCueGapTask = UAbilityTask_WaitDelay::WaitDelay(this, Timeline.GetCurrentDelaySeconds(PizzaPatternDefinition.MakeCueTimings()));
	TelegraphCueGapTask->OnFinish.AddDynamic(this, &ThisClass::HandleTelegraphCueGapFinished);
	TelegraphCueGapTask->ReadyForActivation();
}

void URSGameplayAbility_PizzaPattern::HandleTelegraphCueGapFinished()
{
	TelegraphCueGapTask = nullptr;
	if (bIsCleaningUp || !IsActive())
	{
		return;
	}

	if (!Timeline.Advance() || !BeginCurrentTelegraphCue())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

void URSGameplayAbility_PizzaPattern::BeginRecallDelay()
{
	if (Timeline.GetPhase() != ERSPizzaCueTimelinePhase::RecallDelay)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	const float RecallDelay = Timeline.GetCurrentDelaySeconds(PizzaPatternDefinition.MakeCueTimings());
	if (RecallDelay <= 0.0f)
	{
		HandleRecallDelayFinished();

		return;
	}

	RecallDelayTask = UAbilityTask_WaitDelay::WaitDelay(this, RecallDelay);
	RecallDelayTask->OnFinish.AddDynamic(this, &ThisClass::HandleRecallDelayFinished);
	RecallDelayTask->ReadyForActivation();
}

void URSGameplayAbility_PizzaPattern::HandleRecallDelayFinished()
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

void URSGameplayAbility_PizzaPattern::RunCurrentExplosion()
{
	if (!ExecuteCurrentExplosion())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	// 폭발은 빗나가도 보여야 하므로 적중 여부와 무관하게 재생합니다
	PlayCurrentExplosionPresentation();
	ActiveSliceTransforms.Reset();

	if (!Timeline.Advance())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	if (Timeline.GetPhase() == ERSPizzaCueTimelinePhase::Complete)
	{
		// 마지막 판정은 끝났지만 Montage가 남아 있으면 잘리지 않도록 종료를 미룹니다
		FinishPatternWhenMontageEnds();

		return;
	}

	ExplosionIntervalTask = UAbilityTask_WaitDelay::WaitDelay(this, Timeline.GetCurrentDelaySeconds(PizzaPatternDefinition.MakeCueTimings()));
	ExplosionIntervalTask->OnFinish.AddDynamic(this, &ThisClass::HandleExplosionIntervalFinished);
	ExplosionIntervalTask->ReadyForActivation();
}

void URSGameplayAbility_PizzaPattern::HandleExplosionIntervalFinished()
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

bool URSGameplayAbility_PizzaPattern::ExecuteCurrentExplosion()
{
	const int32 ExplosionIndex = Timeline.GetSequenceIndex();
	AActor* AvatarActor = CurrentActorInfo ? CurrentActorInfo->AvatarActor.Get() : nullptr;
	if (Timeline.GetPhase() != ERSPizzaCueTimelinePhase::Explosion || !AvatarActor
		|| !PizzaPatternDefinition.TryBuildExplosionSliceTransforms(LockedPatternTransform, ExplosionIndex, ActiveSliceTransforms))
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
	CandidateShape.OuterRadius = PizzaPatternDefinition.OuterRadius;
	// 후보를 모으는 원반도 조각과 같은 안쪽 경계를 써야 보스 발밑이 판정에서 함께 빠집니다
	CandidateShape.InnerRadius = ActiveSliceShape.InnerRadius;

	TArray<AActor*> CandidateTargets;
	URSCombatFunctionLibrary::FindTargetsInShape(AvatarActor, TargetChannel, CandidateShape, LockedPatternTransform, CandidateTargets);

	HitActors.Reset();
	for (AActor* CandidateTarget : CandidateTargets)
	{
		const TWeakObjectPtr<AActor> TargetPointer(CandidateTarget);
		if (!CandidateTarget || HitActors.Contains(TargetPointer)
			|| !PizzaPatternDefinition.IsLocationInExplosionGroup(LockedPatternTransform, ExplosionIndex, CandidateTarget->GetActorLocation()))
		{
			continue;
		}

		HitActors.Add(TargetPointer);
		ApplyPatternHitToTarget(CandidateTarget);
	}

	return true;
}

void URSGameplayAbility_PizzaPattern::PlayCurrentExplosionPresentation()
{
	// 예고와 같은 조각 형상을 넘기며, Niagara는 조각마다 나지만 소리와 셰이크는 한 번의 폭발에 하나여야 합니다
	PlayPatternPresentation(ActiveSliceShape, ActiveSliceTransforms, LockedPatternTransform.GetLocation());
}

void URSGameplayAbility_PizzaPattern::HideActiveTelegraphs()
{
	AActor* AvatarActor = CurrentActorInfo ? CurrentActorInfo->AvatarActor.Get() : nullptr;
	URSAttackTelegraphComponent* TelegraphComp = AvatarActor ? AvatarActor->FindComponentByClass<URSAttackTelegraphComponent>() : nullptr;
	if (TelegraphComp)
	{
		for (int32 TelegraphHandle : ActiveTelegraphHandles)
		{
			TelegraphComp->HideShape(TelegraphHandle);
		}
	}

	ActiveTelegraphHandles.Reset();
}

#if WITH_EDITOR
EDataValidationResult URSGameplayAbility_PizzaPattern::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult ValidationResult = Super::IsDataValid(Context);

	FString ValidationError;
	if (!PizzaPatternDefinition.IsDataValid(&ValidationError))
	{
		Context.AddError(FText::FromString(FString::Printf(TEXT("PizzaPatternDefinition is invalid: %s"), *ValidationError)));
		ValidationResult = EDataValidationResult::Invalid;
	}
	else
	{
		// 조각은 전부 같은 반지름과 각도를 쓰므로 조각 하나로 연출 채우기 설정을 대표해 검사합니다
		// 안쪽 경계를 에셋이 소유하므로 런타임과 같은 형상으로 검사합니다
		FRSCombatShape SliceShape;
		if (PizzaPatternDefinition.TryMakeSliceShape(SliceShape))
		{
			ValidationResult = CombineDataValidationResults(ValidationResult, ValidatePatternPresentation(SliceShape, Context));
		}
	}

	const FGameplayTagContainer& AssetTags = GetAssetTags();
	const bool bHasOneExplosionTag = AssetTags.HasTagExact(RSGameplayTags::Ability_Combat_PizzaPattern_OneExplosion);
	const bool bHasTwoExplosionsTag = AssetTags.HasTagExact(RSGameplayTags::Ability_Combat_PizzaPattern_TwoExplosions);
	if (bHasOneExplosionTag == bHasTwoExplosionsTag)
	{
		Context.AddError(FText::FromString(TEXT("Pizza Pattern Ability must have exactly one OneExplosion or TwoExplosions leaf AssetTag.")));
		ValidationResult = EDataValidationResult::Invalid;
	}
	else if ((bHasOneExplosionTag && PizzaPatternDefinition.ExplosionCount != 1)
		|| (bHasTwoExplosionsTag && PizzaPatternDefinition.ExplosionCount != 2))
	{
		Context.AddError(FText::FromString(TEXT("ExplosionCount must exactly match the OneExplosion or TwoExplosions Pizza Pattern leaf AssetTag.")));
		ValidationResult = EDataValidationResult::Invalid;
	}

	return ValidationResult;
}
#endif
