// Fill out your copyright notice in the Description page of Project Settings.

#include "RSGameplayAbility_ArmSwing.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Tasks/RSAbilityTask_ObserveAttackWindow.h"
#include "Tasks/RSAbilityTask_BossFacing.h"
#include "RSAttackTelegraphComponent.h"
#include "RSAttackWindowMath.h"
#include "RSBossCharacter.h"
#include "RSBossController.h"
#include "RSGameplayTags.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace
{
	/** 진행률 Curve의 단조 증가와 양끝 값을 검사할 때 Attack Window를 나눌 구간 수입니다 */
	constexpr int32 SweepCurveValidationSampleCount = 32;
}

const FName URSGameplayAbility_ArmSwing::SweepProgressCurveName(TEXT("ArmSwingSweepProgress"));

FRSArmSwingPathDefinition FRSArmSwingVariantDefinition::GetPathDefinition() const
{
	FRSArmSwingPathDefinition PathDefinition;
	PathDefinition.StartYawOffset = StartYawOffset;
	PathDefinition.TravelAngleDegrees = TravelAngleDegrees;

	return PathDefinition;
}

URSGameplayAbility_ArmSwing::URSGameplayAbility_ArmSwing()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	ActivationBlockedTags.AddTag(RSGameplayTags::State_Action_Locked);
	HitDefinition.Reaction.Type = ERSHitReactionType::Knockdown;
}

void URSGameplayAbility_ArmSwing::BeginPatternTimeline()
{
	ResetTransientState();

	ARSBossCharacter* BossCharacter = nullptr;
	ARSBossController* BossController = nullptr;
	if (!CurrentActorInfo
		|| !CurrentActorInfo->AbilitySystemComponent.IsValid()
		|| !GetBossContext(BossCharacter, BossController)
		|| !BossCharacter->GetCharacterMovement()
		|| !BossCharacter->GetAttackTelegraphComponent()
		|| !IsVariantRuntimeValid(LeftVariant)
		|| !IsVariantRuntimeValid(RightVariant)
		|| LeftVariant.AttackMontage == RightVariant.AttackMontage
		|| !AttackSector.IsDataValid()
		|| !HitDefinition.DamageEffectClass
		|| HitDefinition.Reaction.Type != ERSHitReactionType::Knockdown
		|| !FMath::IsFinite(HitDefinition.Reaction.KnockbackDistance)
		|| !FMath::IsFinite(HitDefinition.Reaction.KnockbackHeight)
		|| !FMath::IsFinite(HitDefinition.Reaction.KnockbackDuration)
		|| !FMath::IsFinite(MontagePlayRate)
		|| MontagePlayRate <= 0.0f
		|| HitDefinition.Reaction.KnockbackDistance < 0.0f
		|| HitDefinition.Reaction.KnockbackHeight < 0.0f
		|| HitDefinition.Reaction.KnockbackDuration <= 0.0f
		|| !FMath::IsFinite(AimRotationSpeed)
		|| !FMath::IsFinite(AimYawTolerance)
		|| !FMath::IsFinite(TargetDriftTolerance)
		|| !FMath::IsFinite(MaxAimDuration)
		|| AimRotationSpeed <= 0.0f
		|| AimYawTolerance < 0.0f
		|| TargetDriftTolerance < 0.0f
		|| MaxAimDuration <= 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s cannot activate Arm Swing because its runtime context or configuration is invalid"), *GetName());
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	AActor* TargetActor = BossController->GetTargetActor();
	if (!BossController->IsTargetActorValid(TargetActor))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	SelectedVariant = FMath::RandBool() ? &LeftVariant : &RightVariant;

	// 검증과 선택이 끝나기 전에 Focus나 회전 설정을 바꾸면 실패한 활성화가 외부 상태를 남깁니다
	if (!CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}
	bHasCommittedActivation = true;

	State = ERSArmSwingState::PreAiming;
	const FRSBossFacingRequest FacingRequest = FRSBossFacingRequest::MakeTrackActor(TargetActor, AimRotationSpeed, AimYawTolerance, TargetDriftTolerance, MaxAimDuration);
	URSAbilityTask_BossFacing* FacingTask = CreateBossFacingTask(FacingRequest);
	if (!FacingTask)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	FacingTask->OnFacingFinished.AddDynamic(this, &ThisClass::HandleBossFacingFinished);
	FacingTask->ReadyForActivation();
}

void URSGameplayAbility_ArmSwing::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (bIsCleaningUp)
	{
		return;
	}

	bIsCleaningUp = true;
	State = ERSArmSwingState::Inactive;

	ARSBossCharacter* BossCharacter = nullptr;
	ARSBossController* BossController = nullptr;
	GetBossContext(BossCharacter, BossController);
	if (BossCharacter && ActiveTelegraphHandle != INDEX_NONE)
	{
		if (URSAttackTelegraphComponent* TelegraphComp = BossCharacter->GetAttackTelegraphComponent())
		{
			TelegraphComp->HideShape(ActiveTelegraphHandle);
		}
	}

	if (AttackWindowTask)
	{
		AttackWindowTask->EndTask();
	}

	if (bHasCommittedActivation && SelectedVariant)
	{
		EndAnimationGameplayStatesForMontage(ActorInfo, SelectedVariant->AttackMontage);
	}

	ResetTransientState();
	bIsCleaningUp = true;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	// 정리가 끝났으므로 플래그를 되돌립니다. 남겨 두면 다음 활성화의 선딜 구간에서 취소가 이 가드에 막혀 어빌리티가 끝나지 않습니다
	bIsCleaningUp = false;
}

void URSGameplayAbility_ArmSwing::HandleBossFacingFinished(ERSBossFacingResult Result, float FinalYawDegrees)
{
	if (bIsCleaningUp || State != ERSArmSwingState::PreAiming)
	{
		return;
	}

	if (Result == ERSBossFacingResult::Aligned || Result == ERSBossFacingResult::NoDirection)
	{
		ConfirmAttack();
		return;
	}

	// 공격 전 회전 완료가 필수이므로 timeout과 Target 소실에서는 공격하지 않습니다
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void URSGameplayAbility_ArmSwing::ConfirmAttack()
{
	if (bIsCleaningUp || State != ERSArmSwingState::PreAiming || !SelectedVariant)
	{
		return;
	}

	ARSBossCharacter* BossCharacter = nullptr;
	ARSBossController* BossController = nullptr;
	if (!GetBossContext(BossCharacter, BossController))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	UCapsuleComponent* CapsuleComp = BossCharacter->GetCapsuleComponent();
	if (!CapsuleComp || !FRSArmSwingMath::TryCalculateLockedAttackTransform(CapsuleComp->GetComponentTransform(), CapsuleComp->GetScaledCapsuleHalfHeight(), BossCharacter->GetActorForwardVector(), LockedAttackTransform))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	State = ERSArmSwingState::Attacking;
	StartAttackMontage();
}

void URSGameplayAbility_ArmSwing::StartAttackMontage()
{
	if (bIsCleaningUp || State != ERSArmSwingState::Attacking || !SelectedVariant || !SelectedVariant->AttackMontage)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, SelectedVariant->AttackMontage, MontagePlayRate);
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleAttackMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleAttackMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleAttackMontageInterrupted);
	MontageTask->ReadyForActivation();

	if (bIsCleaningUp || State != ERSArmSwingState::Attacking)
	{
		return;
	}

	ARSBossCharacter* BossCharacter = nullptr;
	ARSBossController* BossController = nullptr;
	UAnimInstance* AnimInstance = CurrentActorInfo ? CurrentActorInfo->GetAnimInstance() : nullptr;
	FRSAnnularSectorBounds TelegraphBounds;
	if (!GetBossContext(BossCharacter, BossController)
		|| !AnimInstance
		|| !AnimInstance->Montage_IsActive(SelectedVariant->AttackMontage)
		|| !URSAbilityTask_ObserveAttackWindow::TryGetAttackWindowRange(SelectedVariant->AttackMontage, AttackWindowStartPosition, AttackWindowEndPosition)
		|| !FRSArmSwingMath::TryCalculateTelegraphBounds(AttackSector, SelectedVariant->GetPathDefinition(), TelegraphBounds))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	TelegraphStartPosition = AnimInstance->Montage_GetPosition(SelectedVariant->AttackMontage);
	if (AttackWindowStartPosition - TelegraphStartPosition <= KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s requires the Attack Window to begin after the Arm Swing Montage start position"), *GetName());
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	FRSAnnularSectorTelegraphDefinition TelegraphDefinition;
	TelegraphDefinition.InnerRadius = TelegraphBounds.InnerRadius;
	TelegraphDefinition.OuterRadius = TelegraphBounds.OuterRadius;
	TelegraphDefinition.StartYawOffset = TelegraphBounds.StartYawOffset;
	TelegraphDefinition.SweepAngleDegrees = TelegraphBounds.SweepAngleDegrees;
	ActiveTelegraphHandle = BossCharacter->GetAttackTelegraphComponent()->ShowAnnularSector(TelegraphDefinition, LockedAttackTransform);
	if (ActiveTelegraphHandle == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s cannot show its required annular-sector Telegraph"), *GetName());
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	AttackWindowTask = URSAbilityTask_ObserveAttackWindow::ObserveAttackWindow(this, SelectedVariant->AttackMontage);
	AttackWindowTask->OnPositionUpdated.AddDynamic(this, &ThisClass::HandleAttackMontagePositionUpdated);
	AttackWindowTask->OnWindowBegan.AddDynamic(this, &ThisClass::HandleAttackWindowBegan);
	AttackWindowTask->OnWindowAdvanced.AddDynamic(this, &ThisClass::HandleAttackWindowAdvanced);
	AttackWindowTask->OnWindowEnded.AddDynamic(this, &ThisClass::HandleAttackWindowEnded);
	AttackWindowTask->OnInvalidated.AddDynamic(this, &ThisClass::HandleAttackWindowInvalidated);
	AttackWindowTask->ReadyForActivation();
}

void URSGameplayAbility_ArmSwing::HandleAttackMontagePositionUpdated(float MontagePosition)
{
	if (bIsCleaningUp || State != ERSArmSwingState::Attacking || ActiveTelegraphHandle == INDEX_NONE)
	{
		return;
	}

	float Fill = 0.0f;
	ARSBossCharacter* BossCharacter = nullptr;
	ARSBossController* BossController = nullptr;
	if (!FRSAttackWindowMath::TryCalculateAlpha(MontagePosition, TelegraphStartPosition, AttackWindowStartPosition, Fill)
		|| !GetBossContext(BossCharacter, BossController)
		|| !BossCharacter->GetAttackTelegraphComponent()->SetExternalFill(ActiveTelegraphHandle, Fill))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

void URSGameplayAbility_ArmSwing::HandleAttackWindowBegan()
{
	if (bIsCleaningUp || State != ERSArmSwingState::Attacking || bIsAttackWindowActive || bHasCompletedAttackWindow)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	ARSBossCharacter* BossCharacter = nullptr;
	ARSBossController* BossController = nullptr;
	if (!GetBossContext(BossCharacter, BossController)
		|| ActiveTelegraphHandle == INDEX_NONE
		|| !BossCharacter->GetAttackTelegraphComponent()->SetExternalFill(ActiveTelegraphHandle, 1.0f))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	HitActors.Reset();
	bIsAttackWindowActive = true;

	// 팔 휘두르기의 판정은 Window 동안 매 Tick 이어지므로 판정마다 재생하면 연출이 끊임없이 반복됩니다
	// 한 번의 휘두르기를 한 번의 충격으로 보여 주도록 Window가 열리는 순간에만 재생합니다
	PlayPatternPresentation(LockedAttackTransform);

	// 진행률 Curve는 Window 시작에서 0으로 검증되므로 시작점의 공격 폭부터 판정합니다
	float WindowStartSweepProgress = 0.0f;
	if (!TryEvaluateSweepProgress(0.0f, WindowStartSweepProgress) || !ExecuteAttackSectorSlice(WindowStartSweepProgress, WindowStartSweepProgress))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

void URSGameplayAbility_ArmSwing::HandleAttackWindowAdvanced(float PreviousAlpha, float CurrentAlpha)
{
	if (bIsCleaningUp)
	{
		return;
	}

	if (State != ERSArmSwingState::Attacking || !bIsAttackWindowActive || !SelectedVariant)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	float PreviousSweepProgress = 0.0f;
	float CurrentSweepProgress = 0.0f;
	if (!TryEvaluateSweepProgress(PreviousAlpha, PreviousSweepProgress) || !TryEvaluateSweepProgress(CurrentAlpha, CurrentSweepProgress))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	// 직전 각도부터 현재 각도까지 연속된 조각을 검사하므로 프레임 사이를 별도 Sample로 채울 필요가 없습니다
	if (!ExecuteAttackSectorSlice(PreviousSweepProgress, CurrentSweepProgress))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

void URSGameplayAbility_ArmSwing::HandleAttackWindowEnded()
{
	if (bIsCleaningUp || State != ERSArmSwingState::Attacking || !bIsAttackWindowActive)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	bIsAttackWindowActive = false;
	bHasCompletedAttackWindow = true;

	ARSBossCharacter* BossCharacter = nullptr;
	ARSBossController* BossController = nullptr;
	if (GetBossContext(BossCharacter, BossController) && ActiveTelegraphHandle != INDEX_NONE)
	{
		BossCharacter->GetAttackTelegraphComponent()->HideShape(ActiveTelegraphHandle);
	}
	ActiveTelegraphHandle = INDEX_NONE;
	AttackWindowTask = nullptr;
}

void URSGameplayAbility_ArmSwing::HandleAttackWindowInvalidated()
{
	if (!bIsCleaningUp)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

bool URSGameplayAbility_ArmSwing::TryEvaluateSweepProgress(float WindowAlpha, float& OutSweepProgress) const
{
	OutSweepProgress = 0.0f;

	const UAnimMontage* AttackMontage = SelectedVariant ? SelectedVariant->AttackMontage.Get() : nullptr;
	if (!AttackMontage || !FMath::IsFinite(WindowAlpha) || AttackWindowEndPosition - AttackWindowStartPosition <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const float MontagePosition = FMath::Lerp(AttackWindowStartPosition, AttackWindowEndPosition, FMath::Clamp(WindowAlpha, 0.0f, 1.0f));
	const float RawSweepProgress = AttackMontage->EvaluateCurveData(SweepProgressCurveName, FAnimExtractContext(static_cast<double>(MontagePosition)));
	if (!FMath::IsFinite(RawSweepProgress))
	{
		return false;
	}

	OutSweepProgress = FMath::Clamp(RawSweepProgress, 0.0f, 1.0f);

	return true;
}

bool URSGameplayAbility_ArmSwing::ExecuteAttackSectorSlice(float PreviousSweepProgress, float CurrentSweepProgress)
{
	ARSBossCharacter* BossCharacter = nullptr;
	ARSBossController* BossController = nullptr;
	if (!SelectedVariant || !GetBossContext(BossCharacter, BossController))
	{
		return false;
	}

	const FRSArmSwingPathDefinition PathDefinition = SelectedVariant->GetPathDefinition();
	FRSAnnularSectorBounds SectorBounds;
	if (!FRSArmSwingMath::TryCalculateSectorBounds(AttackSector, PathDefinition, PreviousSweepProgress, CurrentSweepProgress, SectorBounds))
	{
		return false;
	}

	// 넓은 원으로 PlayerHurtBox 후보만 모은 뒤 Actor 중심점으로 실제 환형 부채꼴을 판정합니다
	FRSCombatShape CandidateShape;
	CandidateShape.Type = ERSCombatShapeType::AnnularSector;
	CandidateShape.OuterRadius = SectorBounds.OuterRadius + FRSArmSwingMath::CandidateQueryRadiusMargin;
	// 후보를 모으는 원반도 부채꼴과 같은 안쪽 경계를 써야 보스 발밑이 판정에서 함께 빠집니다
	CandidateShape.InnerRadius = SectorBounds.InnerRadius;

	TArray<AActor*> CandidateTargets;
	URSCombatFunctionLibrary::FindTargetsInShapeWithoutDebugDraw(BossCharacter, TargetChannel, CandidateShape, LockedAttackTransform, CandidateTargets);

	const float DamageAmount = GetPatternDamageAmount();
	if (!FMath::IsFinite(DamageAmount))
	{
		return false;
	}

	bool bHasTargetsInSectorSlice = false;
	for (AActor* HitTarget : CandidateTargets)
	{
		const TWeakObjectPtr<AActor> HitTargetPointer(HitTarget);
		if (!HitTarget || !URSCombatFunctionLibrary::IsLocationInsideAnnularSector(LockedAttackTransform, SectorBounds, HitTarget->GetActorLocation()))
		{
			continue;
		}

		bHasTargetsInSectorSlice = true;
		if (HitActors.Contains(HitTargetPointer))
		{
			continue;
		}

		FVector TangentDirection;
		if (!FRSArmSwingMath::TryCalculateTargetTangentDirection(LockedAttackTransform, PathDefinition, CurrentSweepProgress, HitTarget->GetActorLocation(), TangentDirection))
		{
			return false;
		}

		// 면역 결과와 무관하게 이 Window에서 같은 Actor에게 요청을 반복하지 않습니다
		HitActors.Add(HitTargetPointer);
		ApplyPatternHitToTarget(HitTarget, TangentDirection);
	}

	if (URSCombatFunctionLibrary::IsHitCheckDebugEnabled())
	{
		URSCombatFunctionLibrary::DrawDebugCombatAnnularSector(BossCharacter->GetWorld(), LockedAttackTransform, SectorBounds.InnerRadius, SectorBounds.OuterRadius, SectorBounds.StartYawOffset, SectorBounds.SweepAngleDegrees, bHasTargetsInSectorSlice ? FColor::Red : FColor::Silver, 0.0f);
	}

	return true;
}

void URSGameplayAbility_ArmSwing::HandleAttackMontageCompleted()
{
	if (bIsCleaningUp)
	{
		return;
	}

	if (State != ERSArmSwingState::Attacking || !bHasCompletedAttackWindow)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	MontageTask = nullptr;
	FinishPatternWhenMontageEnds();
}

void URSGameplayAbility_ArmSwing::HandleAttackMontageInterrupted()
{
	if (!bIsCleaningUp)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

bool URSGameplayAbility_ArmSwing::GetBossContext(ARSBossCharacter*& OutBossCharacter, ARSBossController*& OutBossController) const
{
	OutBossCharacter = CurrentActorInfo ? Cast<ARSBossCharacter>(CurrentActorInfo->AvatarActor.Get()) : nullptr;
	OutBossController = OutBossCharacter ? Cast<ARSBossController>(OutBossCharacter->GetController()) : nullptr;

	return OutBossCharacter && OutBossController;
}

bool URSGameplayAbility_ArmSwing::IsVariantRuntimeValid(const FRSArmSwingVariantDefinition& Variant, int32* OutAttackWindowCount) const
{
	int32 AttackWindowCount = 0;
	float WindowStartPosition = 0.0f;
	float WindowEndPosition = 0.0f;
	const bool bHasValidAttackWindow = URSAbilityTask_ObserveAttackWindow::TryGetAttackWindowRange(Variant.AttackMontage, WindowStartPosition, WindowEndPosition, &AttackWindowCount);

	if (OutAttackWindowCount)
	{
		*OutAttackWindowCount = AttackWindowCount;
	}

	return Variant.AttackMontage
		&& Variant.GetPathDefinition().IsDataValid()
		&& bHasValidAttackWindow
		&& WindowStartPosition > KINDA_SMALL_NUMBER
		// Curve가 없으면 EvaluateCurveData가 조용히 0을 돌려주어 Sector가 전혀 진행하지 않습니다
		&& Variant.AttackMontage->HasCurveData(SweepProgressCurveName);
}

bool URSGameplayAbility_ArmSwing::IsVariantSweepCurveValid(const FRSArmSwingVariantDefinition& Variant, FString* OutValidationError) const
{
	if (OutValidationError)
	{
		OutValidationError->Reset();
	}

	const UAnimMontage* AttackMontage = Variant.AttackMontage.Get();
	float WindowStartPosition = 0.0f;
	float WindowEndPosition = 0.0f;
	if (!AttackMontage || !URSAbilityTask_ObserveAttackWindow::TryGetAttackWindowRange(AttackMontage, WindowStartPosition, WindowEndPosition))
	{
		if (OutValidationError)
		{
			*OutValidationError = TEXT("Montage와 Attack Window가 있어야 회전 진행률 Curve를 검사할 수 있습니다");
		}

		return false;
	}

	if (!AttackMontage->HasCurveData(SweepProgressCurveName))
	{
		if (OutValidationError)
		{
			*OutValidationError = FString::Printf(TEXT("Montage에 %s Curve가 없습니다"), *SweepProgressCurveName.ToString());
		}

		return false;
	}

	TArray<float> ProgressSamples;
	ProgressSamples.Reserve(SweepCurveValidationSampleCount + 1);
	for (int32 SampleIndex = 0; SampleIndex <= SweepCurveValidationSampleCount; ++SampleIndex)
	{
		const float SampleAlpha = static_cast<float>(SampleIndex) / static_cast<float>(SweepCurveValidationSampleCount);
		const float SamplePosition = FMath::Lerp(WindowStartPosition, WindowEndPosition, SampleAlpha);
		ProgressSamples.Add(AttackMontage->EvaluateCurveData(SweepProgressCurveName, FAnimExtractContext(static_cast<double>(SamplePosition))));
	}

	return FRSArmSwingMath::IsProgressSampleSequenceValid(ProgressSamples, OutValidationError);
}

void URSGameplayAbility_ArmSwing::ResetTransientState()
{
	SelectedVariant = nullptr;
	LockedAttackTransform = FTransform::Identity;
	bIsCleaningUp = false;
	bHasCommittedActivation = false;
	State = ERSArmSwingState::Inactive;
	bIsAttackWindowActive = false;
	bHasCompletedAttackWindow = false;
	ActiveTelegraphHandle = INDEX_NONE;
	TelegraphStartPosition = 0.0f;
	AttackWindowStartPosition = 0.0f;
	AttackWindowEndPosition = 0.0f;
	HitActors.Reset();
	MontageTask = nullptr;
	AttackWindowTask = nullptr;
}

#if WITH_EDITOR
EDataValidationResult URSGameplayAbility_ArmSwing::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult ValidationResult = Super::IsDataValid(Context);

	int32 LeftAttackWindowCount = 0;
	int32 RightAttackWindowCount = 0;
	if (!IsVariantRuntimeValid(LeftVariant, &LeftAttackWindowCount))
	{
		Context.AddError(FText::FromString(FString::Printf(TEXT("LeftVariant requires a valid path, Montage, a %s Curve, and exactly one Attack Window that begins after the Montage start, but it has %d Windows."), *SweepProgressCurveName.ToString(), LeftAttackWindowCount)));
		ValidationResult = EDataValidationResult::Invalid;
	}

	if (!IsVariantRuntimeValid(RightVariant, &RightAttackWindowCount))
	{
		Context.AddError(FText::FromString(FString::Printf(TEXT("RightVariant requires a valid path, Montage, a %s Curve, and exactly one Attack Window that begins after the Montage start, but it has %d Windows."), *SweepProgressCurveName.ToString(), RightAttackWindowCount)));
		ValidationResult = EDataValidationResult::Invalid;
	}

	FString LeftSweepCurveValidationError;
	if (!IsVariantSweepCurveValid(LeftVariant, &LeftSweepCurveValidationError))
	{
		Context.AddError(FText::FromString(FString::Printf(TEXT("LeftVariant sweep progress curve is invalid: %s"), *LeftSweepCurveValidationError)));
		ValidationResult = EDataValidationResult::Invalid;
	}

	FString RightSweepCurveValidationError;
	if (!IsVariantSweepCurveValid(RightVariant, &RightSweepCurveValidationError))
	{
		Context.AddError(FText::FromString(FString::Printf(TEXT("RightVariant sweep progress curve is invalid: %s"), *RightSweepCurveValidationError)));
		ValidationResult = EDataValidationResult::Invalid;
	}

	if (LeftVariant.AttackMontage && LeftVariant.AttackMontage == RightVariant.AttackMontage)
	{
		Context.AddError(FText::FromString(TEXT("LeftVariant and RightVariant must use different Montages.")));
		ValidationResult = EDataValidationResult::Invalid;
	}

	FString AttackSectorValidationError;
	if (!AttackSector.IsDataValid(&AttackSectorValidationError))
	{
		Context.AddError(FText::FromString(FString::Printf(TEXT("AttackSector is invalid: %s"), *AttackSectorValidationError)));
		ValidationResult = EDataValidationResult::Invalid;
	}

	if (!FMath::IsFinite(MontagePlayRate) || MontagePlayRate <= 0.0f)
	{
		Context.AddError(FText::FromString(TEXT("MontagePlayRate must be finite and greater than zero.")));
		ValidationResult = EDataValidationResult::Invalid;
	}

	if (HitDefinition.Reaction.Type != ERSHitReactionType::Knockdown
		|| !FMath::IsFinite(HitDefinition.Reaction.KnockbackDistance)
		|| !FMath::IsFinite(HitDefinition.Reaction.KnockbackHeight)
		|| !FMath::IsFinite(HitDefinition.Reaction.KnockbackDuration)
		|| HitDefinition.Reaction.KnockbackDistance < 0.0f
		|| HitDefinition.Reaction.KnockbackHeight < 0.0f
		|| HitDefinition.Reaction.KnockbackDuration <= 0.0f)
	{
		Context.AddError(FText::FromString(TEXT("Reaction must be a Knockdown with finite non-negative distance and height, and positive duration.")));
		ValidationResult = EDataValidationResult::Invalid;
	}

	if (!FMath::IsFinite(AimRotationSpeed)
		|| !FMath::IsFinite(AimYawTolerance)
		|| !FMath::IsFinite(TargetDriftTolerance)
		|| !FMath::IsFinite(MaxAimDuration)
		|| AimRotationSpeed <= 0.0f
		|| AimYawTolerance < 0.0f
		|| TargetDriftTolerance < 0.0f
		|| MaxAimDuration <= 0.0f)
	{
		Context.AddError(FText::FromString(TEXT("Aim values are outside their supported ranges.")));
		ValidationResult = EDataValidationResult::Invalid;
	}

	const FGameplayTagContainer& AssetTags = GetAssetTags();
	const bool bHasSlowTag = AssetTags.HasTagExact(RSGameplayTags::Ability_Combat_ArmSwing_Slow);
	const bool bHasFastTag = AssetTags.HasTagExact(RSGameplayTags::Ability_Combat_ArmSwing_Fast);
	if (bHasSlowTag == bHasFastTag)
	{
		Context.AddError(FText::FromString(TEXT("Arm Swing Ability must have exactly one Slow or Fast leaf AssetTag.")));
		ValidationResult = EDataValidationResult::Invalid;
	}

	return ValidationResult;
}
#endif
