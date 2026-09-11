#include "RSGameplayAbility_SequentialSweepExplosion.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Combat/RSSequentialSweepExplosionMath.h"
#include "RSAttackTelegraphComponent.h"
#include "RSBossCharacter.h"
#include "RSBossController.h"
#include "RSGameplayTags.h"
#include "Tasks/RSAbilityTask_ObserveElapsedTime.h"
#include "Tasks/RSAbilityTask_ObserveFacing.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

bool FRSSequentialSweepExplosionDefinition::IsDataValid(FString* OutValidationError) const
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

	if (!FMath::IsFinite(OuterRadius) || OuterRadius <= 0.0f)
	{
		SetValidationError(TEXT("OuterRadius must be finite and greater than zero."));

		return false;
	}

	if (!FMath::IsFinite(TotalSweepAngleDegrees) || TotalSweepAngleDegrees <= 0.0f || TotalSweepAngleDegrees > 360.0f)
	{
		SetValidationError(TEXT("TotalSweepAngleDegrees must be finite and satisfy 0 < angle <= 360."));

		return false;
	}

	if (SectorCount < 1)
	{
		SetValidationError(TEXT("SectorCount must be at least 1."));

		return false;
	}

	if (!FMath::IsFinite(StartAngleOffsetDegrees))
	{
		SetValidationError(TEXT("StartAngleOffsetDegrees must be finite."));

		return false;
	}

	if (!FMath::IsFinite(FirstExplosionDelay) || FirstExplosionDelay <= 0.0f)
	{
		SetValidationError(TEXT("FirstExplosionDelay must be finite and greater than zero."));

		return false;
	}

	if (!FMath::IsFinite(SectorExplosionInterval) || SectorExplosionInterval <= 0.0f)
	{
		SetValidationError(TEXT("SectorExplosionInterval must be finite and greater than zero."));

		return false;
	}

	// 경고 간격 안에 들어가야 아직 표시되지도 않은 부채꼴을 지우는 순서 역전이 생기지 않습니다
	const float WarningInterval = FirstExplosionDelay / static_cast<float>(SectorCount);
	if (!FMath::IsFinite(TelegraphHideLeadTime) || TelegraphHideLeadTime < 0.0f || TelegraphHideLeadTime >= WarningInterval)
	{
		SetValidationError(TEXT("TelegraphHideLeadTime must be finite, zero or greater, and shorter than the warning interval."));

		return false;
	}

	const float DamageValue = Damage.GetValueAtLevel(1.0f);
	constexpr float DamageIntegerTolerance = 0.01f;
	if (!FMath::IsFinite(DamageValue) || DamageValue < 0.0f || !FMath::IsNearlyEqual(DamageValue, FMath::RoundToFloat(DamageValue), DamageIntegerTolerance))
	{
		SetValidationError(TEXT("Damage must evaluate to a finite non-negative integer at level 1."));

		return false;
	}

	if (Reaction.Type == ERSHitReactionType::Knockdown
		&& (!FMath::IsFinite(Reaction.KnockbackDistance) || Reaction.KnockbackDistance < 0.0f
			|| !FMath::IsFinite(Reaction.KnockbackHeight) || Reaction.KnockbackHeight < 0.0f
			|| !FMath::IsFinite(Reaction.KnockbackDuration) || Reaction.KnockbackDuration <= 0.0f))
	{
		SetValidationError(TEXT("Knockdown reaction values are outside their supported ranges."));

		return false;
	}

	return true;
}

URSGameplayAbility_SequentialSweepExplosion::URSGameplayAbility_SequentialSweepExplosion()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(RSGameplayTags::Ability_Combat_SequentialSweepExplosion);
	SetAssetTags(AssetTags);
	ActivationBlockedTags.AddTag(RSGameplayTags::State_Action_Locked);
}

void URSGameplayAbility_SequentialSweepExplosion::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	ResetTransientState();
	bIsCleaningUp = false;

	ARSBossCharacter* BossCharacter = nullptr;
	ARSBossController* BossController = nullptr;
	if (!GetBossContext(BossCharacter, BossController)
		|| !ActorInfo
		|| !ActorInfo->AbilitySystemComponent.IsValid()
		|| !BossCharacter->GetCharacterMovement()
		|| !BossCharacter->GetAttackTelegraphComponent()
		|| !PatternDefinition.IsDataValid()
		|| !DamageEffectClass
		|| !FMath::IsFinite(MontagePlayRate) || MontagePlayRate <= 0.0f
		|| !FMath::IsFinite(AimRotationSpeed) || AimRotationSpeed <= 0.0f
		|| !FMath::IsFinite(AimYawTolerance) || AimYawTolerance < 0.0f
		|| !FMath::IsFinite(TargetDriftTolerance) || TargetDriftTolerance < 0.0f
		|| !FMath::IsFinite(MaxAimDuration) || MaxAimDuration <= 0.0f)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	AActor* InitialTargetActor = BossController->GetTargetActor();
	if (!BossController->IsTargetActorValid(InitialTargetActor))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}
	bHasCommittedActivation = true;

	UCharacterMovementComponent* CharacterMovementComp = BossCharacter->GetCharacterMovement();
	OriginalRotationRate = CharacterMovementComp->RotationRate;
	bOriginalUseControllerDesiredRotation = CharacterMovementComp->bUseControllerDesiredRotation;
	bHasSavedRotationSettings = true;
	CharacterMovementComp->RotationRate.Yaw = AimRotationSpeed;
	CharacterMovementComp->bUseControllerDesiredRotation = true;

	BeginAim();
}

void URSGameplayAbility_SequentialSweepExplosion::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (bIsCleaningUp)
	{
		return;
	}

	bIsCleaningUp = true;
	State = ERSSequentialSweepExplosionState::Inactive;

	ARSBossCharacter* BossCharacter = nullptr;
	ARSBossController* BossController = nullptr;
	GetBossContext(BossCharacter, BossController);

	if (ObserveFacingTask)
	{
		ObserveFacingTask->EndTask();
		ObserveFacingTask = nullptr;
	}

	if (TimelineTask)
	{
		TimelineTask->EndTask();
		TimelineTask = nullptr;
	}

	HideAllWarningSectors();

	if (BossCharacter && bHasSavedRotationSettings)
	{
		if (UCharacterMovementComponent* CharacterMovementComp = BossCharacter->GetCharacterMovement())
		{
			CharacterMovementComp->RotationRate = OriginalRotationRate;
			CharacterMovementComp->bUseControllerDesiredRotation = bOriginalUseControllerDesiredRotation;
		}
	}

	if (BossController && bHasAppliedGameplayFocus)
	{
		BossController->ClearFocus(EAIFocusPriority::Gameplay);
	}

	if (MontageTask)
	{
		MontageTask->EndTask();
		MontageTask = nullptr;
	}

	if (bHasCommittedActivation)
	{
		EndAnimationGameplayStatesForMontage(ActorInfo, AttackMontage);
	}

	AimTargetActor.Reset();
	AimSnapshotLocation = FVector::ZeroVector;
	LockedAttackTransform = FTransform::Identity;
	PreAimStartTime = 0.0f;
	NextWarningSectorIndex = 0;
	NextHideSectorIndex = 0;
	NextExplosionSectorIndex = 0;
	HitActors.Reset();
	bHasSavedRotationSettings = false;
	bHasAppliedGameplayFocus = false;
	bHasCommittedActivation = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	bIsCleaningUp = false;
}

void URSGameplayAbility_SequentialSweepExplosion::BeginAim()
{
	ARSBossCharacter* BossCharacter = nullptr;
	ARSBossController* BossController = nullptr;
	if (!GetBossContext(BossCharacter, BossController))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	AActor* TargetActor = BossController->GetTargetActor();
	if (!BossController->IsTargetActorValid(TargetActor))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	AimTargetActor = TargetActor;
	AimSnapshotLocation = TargetActor->GetActorLocation();
	PreAimStartTime = BossCharacter->GetWorld()->GetTimeSeconds();
	State = ERSSequentialSweepExplosionState::PreAiming;

	BossController->SetFocalPoint(AimSnapshotLocation, EAIFocusPriority::Gameplay);
	bHasAppliedGameplayFocus = true;

	ObserveFacingTask = URSAbilityTask_ObserveFacing::ObserveFacing(this, AimSnapshotLocation, AimYawTolerance);
	ObserveFacingTask->OnFacingUpdated.AddDynamic(this, &ThisClass::HandleFacingUpdated);
	ObserveFacingTask->ReadyForActivation();
}

void URSGameplayAbility_SequentialSweepExplosion::HandleFacingUpdated(bool bHasFacingDirection, bool bIsWithinYawTolerance, float YawErrorDegrees)
{
	if (bIsCleaningUp || State != ERSSequentialSweepExplosionState::PreAiming)
	{
		return;
	}

	ARSBossCharacter* BossCharacter = nullptr;
	ARSBossController* BossController = nullptr;
	AActor* TargetActor = AimTargetActor.Get();
	if (!GetBossContext(BossCharacter, BossController) || !BossController->IsTargetActorValid(TargetActor))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	const float PreAimElapsedTime = BossCharacter->GetWorld()->GetTimeSeconds() - PreAimStartTime;
	if (PreAimElapsedTime >= MaxAimDuration || !bHasFacingDirection)
	{
		ConfirmAttack();

		return;
	}

	if (!bIsWithinYawTolerance)
	{
		return;
	}

	const float DriftDistanceSquared = FVector::DistSquared2D(TargetActor->GetActorLocation(), AimSnapshotLocation);
	if (DriftDistanceSquared > FMath::Square(TargetDriftTolerance))
	{
		AimSnapshotLocation = TargetActor->GetActorLocation();
		BossController->SetFocalPoint(AimSnapshotLocation, EAIFocusPriority::Gameplay);

		if (ObserveFacingTask)
		{
			ObserveFacingTask->SetFacingLocation(AimSnapshotLocation);
		}

		return;
	}

	ConfirmAttack();
}

void URSGameplayAbility_SequentialSweepExplosion::ConfirmAttack()
{
	if (bIsCleaningUp || State != ERSSequentialSweepExplosionState::PreAiming)
	{
		return;
	}

	ARSBossCharacter* BossCharacter = nullptr;
	ARSBossController* BossController = nullptr;
	UCapsuleComponent* CapsuleComp = nullptr;
	if (!GetBossContext(BossCharacter, BossController)
		|| !(CapsuleComp = BossCharacter->GetCapsuleComponent())
		|| !RSSequentialSweepExplosionMath::TryCalculateLockedAttackTransform(CapsuleComp->GetComponentTransform(), CapsuleComp->GetScaledCapsuleHalfHeight(), BossCharacter->GetActorForwardVector(), LockedAttackTransform))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	State = ERSSequentialSweepExplosionState::Attacking;
	if (ObserveFacingTask)
	{
		ObserveFacingTask->EndTask();
		ObserveFacingTask = nullptr;
	}

	if (bHasAppliedGameplayFocus)
	{
		BossController->ClearFocus(EAIFocusPriority::Gameplay);
		bHasAppliedGameplayFocus = false;
	}

	if (UCharacterMovementComponent* CharacterMovementComp = BossCharacter->GetCharacterMovement())
	{
		CharacterMovementComp->bUseControllerDesiredRotation = false;
	}

	StartAttackTimeline();
}

void URSGameplayAbility_SequentialSweepExplosion::StartAttackTimeline()
{
	const float TotalDuration = RSSequentialSweepExplosionMath::CalculateTotalDuration(PatternDefinition.FirstExplosionDelay, PatternDefinition.SectorExplosionInterval, PatternDefinition.SectorCount);
	if (TotalDuration <= 0.0f)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	WarningSectorHandles.Init(INDEX_NONE, PatternDefinition.SectorCount);
	RequestAttackMontage();

	TimelineTask = URSAbilityTask_ObserveElapsedTime::ObserveElapsedTime(this, TotalDuration);
	TimelineTask->OnElapsedTimeUpdated.AddDynamic(this, &ThisClass::HandleTimelineElapsedTimeUpdated);
	TimelineTask->ReadyForActivation();
}

void URSGameplayAbility_SequentialSweepExplosion::RequestAttackMontage()
{
	if (!AttackMontage)
	{
		return;
	}

	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, AttackMontage, MontagePlayRate);
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleAttackMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleAttackMontageInterrupted);
	MontageTask->ReadyForActivation();
}

void URSGameplayAbility_SequentialSweepExplosion::HandleAttackMontageCompleted()
{
	MontageTask = nullptr;
}

void URSGameplayAbility_SequentialSweepExplosion::HandleAttackMontageInterrupted()
{
	if (!bIsCleaningUp && State == ERSSequentialSweepExplosionState::Attacking)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

void URSGameplayAbility_SequentialSweepExplosion::HandleTimelineElapsedTimeUpdated(float ElapsedTime)
{
	if (bIsCleaningUp || State != ERSSequentialSweepExplosionState::Attacking)
	{
		return;
	}

	const int32 RequiredWarningCount = RSSequentialSweepExplosionMath::CalculateRequiredWarningCount(ElapsedTime, PatternDefinition.FirstExplosionDelay, PatternDefinition.SectorCount);
	while (NextWarningSectorIndex < RequiredWarningCount)
	{
		if (!ShowWarningSector(NextWarningSectorIndex))
		{
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

			return;
		}

		++NextWarningSectorIndex;
	}

	// 회수는 폭발을 TelegraphHideLeadTime만큼 앞당긴 것과 같으므로 같은 계산에 시간만 더해 씁니다
	const int32 RequiredHideCount = FMath::Min(RSSequentialSweepExplosionMath::CalculateRequiredExplosionCount(ElapsedTime + PatternDefinition.TelegraphHideLeadTime, PatternDefinition.FirstExplosionDelay, PatternDefinition.SectorExplosionInterval, PatternDefinition.SectorCount), NextWarningSectorIndex);
	while (NextHideSectorIndex < RequiredHideCount)
	{
		if (!HideWarningSector(NextHideSectorIndex))
		{
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

			return;
		}

		++NextHideSectorIndex;
	}

	const int32 RequiredExplosionCount = FMath::Min(RSSequentialSweepExplosionMath::CalculateRequiredExplosionCount(ElapsedTime, PatternDefinition.FirstExplosionDelay, PatternDefinition.SectorExplosionInterval, PatternDefinition.SectorCount), NextHideSectorIndex);
	while (NextExplosionSectorIndex < RequiredExplosionCount)
	{
		if (!ExecuteSectorExplosion(NextExplosionSectorIndex))
		{
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

			return;
		}

		++NextExplosionSectorIndex;
	}

	if (NextExplosionSectorIndex >= PatternDefinition.SectorCount)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

bool URSGameplayAbility_SequentialSweepExplosion::ShowWarningSector(int32 SectorIndex)
{
	ARSBossCharacter* BossCharacter = nullptr;
	ARSBossController* BossController = nullptr;
	float StartAngleOffsetDegrees = 0.0f;
	float SweepAngleDegrees = 0.0f;
	if (!GetBossContext(BossCharacter, BossController)
		|| !WarningSectorHandles.IsValidIndex(SectorIndex)
		|| WarningSectorHandles[SectorIndex] != INDEX_NONE
		|| !RSSequentialSweepExplosionMath::TryCalculateSectorAngles(PatternDefinition.TotalSweepAngleDegrees, PatternDefinition.SectorCount, PatternDefinition.StartAngleOffsetDegrees, SectorIndex, StartAngleOffsetDegrees, SweepAngleDegrees))
	{
		return false;
	}

	FRSCombatShape TelegraphShape;
	TelegraphShape.Type = ERSCombatShapeType::Cone;
	TelegraphShape.Range = PatternDefinition.OuterRadius;
	TelegraphShape.Angle = SweepAngleDegrees;

	// 일반 Cone은 중심축 기준 형상이므로 첫 경계에서 조각 각도의 절반만큼 회전시킵니다
	FTransform TelegraphTransform = LockedAttackTransform;
	const float TelegraphCenterYaw = LockedAttackTransform.GetRotation().Rotator().Yaw + StartAngleOffsetDegrees + SweepAngleDegrees * 0.5f;
	TelegraphTransform.SetRotation(FRotator(0.0f, TelegraphCenterYaw, 0.0f).Quaternion());

	URSAttackTelegraphComponent* TelegraphComp = BossCharacter->GetAttackTelegraphComponent();
	const int32 TelegraphHandle = TelegraphComp ? TelegraphComp->ShowShapeWithExternalFill(TelegraphShape, TelegraphTransform) : INDEX_NONE;
	if (TelegraphHandle == INDEX_NONE || !TelegraphComp->SetExternalFill(TelegraphHandle, 1.0f))
	{
		if (TelegraphComp && TelegraphHandle != INDEX_NONE)
		{
			TelegraphComp->HideShape(TelegraphHandle);
		}

		return false;
	}

	WarningSectorHandles[SectorIndex] = TelegraphHandle;

	return true;
}

bool URSGameplayAbility_SequentialSweepExplosion::HideWarningSector(int32 SectorIndex)
{
	ARSBossCharacter* BossCharacter = nullptr;
	ARSBossController* BossController = nullptr;
	if (!GetBossContext(BossCharacter, BossController) || !WarningSectorHandles.IsValidIndex(SectorIndex))
	{
		return false;
	}

	if (URSAttackTelegraphComponent* TelegraphComp = BossCharacter->GetAttackTelegraphComponent())
	{
		TelegraphComp->HideShape(WarningSectorHandles[SectorIndex]);
	}
	WarningSectorHandles[SectorIndex] = INDEX_NONE;

	return true;
}

bool URSGameplayAbility_SequentialSweepExplosion::ExecuteSectorExplosion(int32 SectorIndex)
{
	ARSBossCharacter* BossCharacter = nullptr;
	ARSBossController* BossController = nullptr;
	if (!GetBossContext(BossCharacter, BossController) || !WarningSectorHandles.IsValidIndex(SectorIndex))
	{
		return false;
	}

	const float DamageAmount = PatternDefinition.Damage.GetValueAtLevel(GetAbilityLevel(CurrentSpecHandle, CurrentActorInfo));
	if (!FMath::IsFinite(DamageAmount) || DamageAmount < 0.0f)
	{
		return false;
	}

	// 공용 Sphere의 외곽은 후보 수집용이므로 1cm 넓게 잡고 실제 포함 여부는 패턴 Math가 확정합니다
	FRSCombatShape CandidateShape;
	CandidateShape.Type = ERSCombatShapeType::Sphere;
	CandidateShape.Radius = PatternDefinition.OuterRadius + 1.0f;
	CandidateShape.InnerRadius = 0.0f;

	TArray<AActor*> CandidateTargets;
	URSCombatFunctionLibrary::FindTargetsInShape(BossCharacter, TargetChannel, CandidateShape, LockedAttackTransform, CandidateTargets);

	HitActors.Reset();
	for (AActor* CandidateTarget : CandidateTargets)
	{
		const TWeakObjectPtr<AActor> TargetPointer(CandidateTarget);
		if (!CandidateTarget || HitActors.Contains(TargetPointer)
			|| !RSSequentialSweepExplosionMath::IsLocationInSector(LockedAttackTransform, PatternDefinition.OuterRadius, PatternDefinition.TotalSweepAngleDegrees, PatternDefinition.SectorCount, PatternDefinition.StartAngleOffsetDegrees, SectorIndex, CandidateTarget->GetActorLocation()))
		{
			continue;
		}

		HitActors.Add(TargetPointer);
		ApplyDamageToTarget(CandidateTarget, DamageEffectClass, DamageAmount);
		URSCombatFunctionLibrary::SendHitReaction(BossCharacter, CandidateTarget, PatternDefinition.Reaction);
	}

	return true;
}

void URSGameplayAbility_SequentialSweepExplosion::HideAllWarningSectors()
{
	AActor* AvatarActor = CurrentActorInfo ? CurrentActorInfo->AvatarActor.Get() : nullptr;
	URSAttackTelegraphComponent* TelegraphComp = AvatarActor ? AvatarActor->FindComponentByClass<URSAttackTelegraphComponent>() : nullptr;
	if (TelegraphComp)
	{
		for (int32 TelegraphHandle : WarningSectorHandles)
		{
			TelegraphComp->HideShape(TelegraphHandle);
		}
	}

	WarningSectorHandles.Reset();
}

bool URSGameplayAbility_SequentialSweepExplosion::GetBossContext(ARSBossCharacter*& OutBossCharacter, ARSBossController*& OutBossController) const
{
	OutBossCharacter = CurrentActorInfo ? Cast<ARSBossCharacter>(CurrentActorInfo->AvatarActor.Get()) : nullptr;
	OutBossController = OutBossCharacter ? Cast<ARSBossController>(OutBossCharacter->GetController()) : nullptr;

	return OutBossCharacter && OutBossController;
}

void URSGameplayAbility_SequentialSweepExplosion::ResetTransientState()
{
	AimTargetActor.Reset();
	AimSnapshotLocation = FVector::ZeroVector;
	LockedAttackTransform = FTransform::Identity;
	PreAimStartTime = 0.0f;
	NextWarningSectorIndex = 0;
	NextHideSectorIndex = 0;
	NextExplosionSectorIndex = 0;
	WarningSectorHandles.Reset();
	HitActors.Reset();
	bHasSavedRotationSettings = false;
	bHasAppliedGameplayFocus = false;
	bHasCommittedActivation = false;
	OriginalRotationRate = FRotator::ZeroRotator;
	bOriginalUseControllerDesiredRotation = false;
	State = ERSSequentialSweepExplosionState::Inactive;
	ObserveFacingTask = nullptr;
	TimelineTask = nullptr;
	MontageTask = nullptr;
}

#if WITH_EDITOR
EDataValidationResult URSGameplayAbility_SequentialSweepExplosion::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult ValidationResult = Super::IsDataValid(Context);

	FString ValidationError;
	if (!PatternDefinition.IsDataValid(&ValidationError))
	{
		Context.AddError(FText::FromString(FString::Printf(TEXT("PatternDefinition is invalid: %s"), *ValidationError)));
		ValidationResult = EDataValidationResult::Invalid;
	}

	if (!DamageEffectClass)
	{
		Context.AddError(FText::FromString(TEXT("DamageEffectClass is required.")));
		ValidationResult = EDataValidationResult::Invalid;
	}

	if (!FMath::IsFinite(MontagePlayRate) || MontagePlayRate <= 0.0f
		|| !FMath::IsFinite(AimRotationSpeed) || AimRotationSpeed <= 0.0f
		|| !FMath::IsFinite(AimYawTolerance) || AimYawTolerance < 0.0f
		|| !FMath::IsFinite(TargetDriftTolerance) || TargetDriftTolerance < 0.0f
		|| !FMath::IsFinite(MaxAimDuration) || MaxAimDuration <= 0.0f)
	{
		Context.AddError(FText::FromString(TEXT("Aim and Montage values are outside their supported ranges.")));
		ValidationResult = EDataValidationResult::Invalid;
	}

	if (!URSCombatFunctionLibrary::ValidateIntegerDamage(PatternDefinition.Damage, TEXT("PatternDefinition.Damage"), Context))
	{
		ValidationResult = EDataValidationResult::Invalid;
	}

	return ValidationResult;
}
#endif
