#include "RSGameplayAbility_TargetedSlam.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Tasks/RSAbilityTask_ObserveFacing.h"
#include "RSAttackTelegraphComponent.h"
#include "RSBossCharacter.h"
#include "RSBossController.h"
#include "RSGameplayTags.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

URSGameplayAbility_TargetedSlam::URSGameplayAbility_TargetedSlam()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	ActivationBlockedTags.AddTag(RSGameplayTags::State_Action_Locked);

	AttackShape.Type = ERSCombatShapeType::Cone;
	AttackShape.Range = 600.0f;
	AttackShape.Angle = 60.0f;
	Reaction.Type = ERSHitReactionType::None;
}

void URSGameplayAbility_TargetedSlam::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	AimTargetActor.Reset();
	AimSnapshotLocation = FVector::ZeroVector;
	LockedAttackTransform = FTransform::Identity;
	PreAimStartTime = 0.0f;
	CurrentStrikeIndex = 0;
	bHasExecutedImpact = false;
	bHasCompletedMontage = false;
	bHasSavedRotationSettings = false;
	bHasAppliedGameplayFocus = false;
	bIsCleaningUp = false;
	bHasCommittedActivation = false;
	State = ERSTargetedSlamState::Inactive;
	ObserveFacingTask = nullptr;
	ImpactDelayTask = nullptr;
	MontageTask = nullptr;

	ARSBossCharacter* BossCharacter = nullptr;
	ARSBossController* BossController = nullptr;
	if (!GetBossContext(BossCharacter, BossController)
		|| !ActorInfo
		|| !ActorInfo->AbilitySystemComponent.IsValid()
		|| !BossCharacter->GetCharacterMovement()
		|| !BossCharacter->GetAttackTelegraphComponent()
		|| !AttackMontage
		|| !DamageEffectClass
		|| StrikeCount < 1
		|| StrikeCount > 2
		|| AttackShape.Type != ERSCombatShapeType::Cone
		|| !AttackShape.IsDataValid()
		|| AimRotationSpeed <= 0.0f
		|| AimYawTolerance < 0.0f
		|| TargetDriftTolerance < 0.0f
		|| MaxAimDuration <= 0.0f
		|| !FMath::IsFinite(MontagePlayRate)
		|| MontagePlayRate <= 0.0f
		|| !FMath::IsFinite(ImpactDelay)
		|| ImpactDelay <= 0.0f
		|| ImpactDelay > AttackMontage->GetPlayLength() / MontagePlayRate)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s cannot activate Targeted Slam because its runtime context or configuration is invalid"), *GetName());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	AActor* InitialTargetActor = BossController->GetTargetActor();
	if (!BossController->IsTargetActorValid(InitialTargetActor))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	// 검증이 끝나기 전에 Focus, 회전 설정이나 Telegraph를 바꾸면 실패한 활성화가 외부 상태를 남깁니다
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

	BeginStrike();
}

void URSGameplayAbility_TargetedSlam::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (bIsCleaningUp)
	{
		return;
	}

	bIsCleaningUp = true;
	State = ERSTargetedSlamState::Inactive;

	ARSBossCharacter* BossCharacter = nullptr;
	ARSBossController* BossController = nullptr;
	GetBossContext(BossCharacter, BossController);

	if (BossCharacter && bHasCommittedActivation)
	{
		if (URSAttackTelegraphComponent* TelegraphComp = BossCharacter->GetAttackTelegraphComponent())
		{
			TelegraphComp->HideAllShapes();
		}

		if (bHasSavedRotationSettings)
		{
			if (UCharacterMovementComponent* CharacterMovementComp = BossCharacter->GetCharacterMovement())
			{
				CharacterMovementComp->RotationRate = OriginalRotationRate;
				CharacterMovementComp->bUseControllerDesiredRotation = bOriginalUseControllerDesiredRotation;
			}
		}
	}

	if (BossController && bHasAppliedGameplayFocus)
	{
		BossController->ClearFocus(EAIFocusPriority::Gameplay);
	}

	if (ObserveFacingTask)
	{
		ObserveFacingTask->EndTask();
	}

	if (bHasCommittedActivation)
	{
		EndAnimationGameplayStatesForMontage(ActorInfo, AttackMontage);
	}

	ResetStrikeTransientState();
	CurrentStrikeIndex = 0;
	bHasSavedRotationSettings = false;
	bHasAppliedGameplayFocus = false;
	bHasCommittedActivation = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void URSGameplayAbility_TargetedSlam::BeginStrike()
{
	if (bIsCleaningUp || CurrentStrikeIndex < 0 || CurrentStrikeIndex >= StrikeCount)
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

	AActor* TargetActor = BossController->GetTargetActor();
	if (!BossController->IsTargetActorValid(TargetActor))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	ResetStrikeTransientState();
	AimTargetActor = TargetActor;
	AimSnapshotLocation = TargetActor->GetActorLocation();
	PreAimStartTime = BossCharacter->GetWorld()->GetTimeSeconds();
	State = ERSTargetedSlamState::PreAiming;

	if (UCharacterMovementComponent* CharacterMovementComp = BossCharacter->GetCharacterMovement())
	{
		CharacterMovementComp->bUseControllerDesiredRotation = true;
	}

	BossController->SetFocalPoint(AimSnapshotLocation, EAIFocusPriority::Gameplay);
	bHasAppliedGameplayFocus = true;

	ObserveFacingTask = URSAbilityTask_ObserveFacing::ObserveFacing(this, AimSnapshotLocation, AimYawTolerance);
	ObserveFacingTask->OnFacingUpdated.AddDynamic(this, &ThisClass::HandleFacingUpdated);
	ObserveFacingTask->ReadyForActivation();
}

void URSGameplayAbility_TargetedSlam::HandleFacingUpdated(bool bHasFacingDirection, bool bIsWithinYawTolerance, float YawErrorDegrees)
{
	if (bIsCleaningUp || State != ERSTargetedSlamState::PreAiming)
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
	if (PreAimElapsedTime >= MaxAimDuration)
	{
		ConfirmAttack();

		return;
	}

	// 같은 XY 위치에는 정해야 할 방향이 없으므로 현재 Facing으로 바로 공격합니다
	if (!bHasFacingDirection)
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

void URSGameplayAbility_TargetedSlam::ConfirmAttack()
{
	if (bIsCleaningUp || State != ERSTargetedSlamState::PreAiming)
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
	FVector HorizontalForward = BossCharacter->GetActorForwardVector();
	HorizontalForward.Z = 0.0f;
	if (!CapsuleComp || !HorizontalForward.Normalize())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	const FVector AttackOrigin = CapsuleComp->GetComponentLocation() - CapsuleComp->GetUpVector() * CapsuleComp->GetScaledCapsuleHalfHeight();
	LockedAttackTransform = FTransform(HorizontalForward.Rotation(), AttackOrigin);
	State = ERSTargetedSlamState::Attacking;

	if (ObserveFacingTask)
	{
		ObserveFacingTask->EndTask();
		ObserveFacingTask = nullptr;
	}

	// 공격 공간은 이미 고정되었으므로 Controller가 움직이는 Target을 따라 Boss Yaw를 다시 바꾸지 않게 합니다
	// Montage가 소유한 Root Motion과 애니메이션 회전은 이 설정과 별개로 허용됩니다
	if (UCharacterMovementComponent* CharacterMovementComp = BossCharacter->GetCharacterMovement())
	{
		CharacterMovementComp->bUseControllerDesiredRotation = false;
	}

	StartStrike();
}

void URSGameplayAbility_TargetedSlam::StartStrike()
{
	ARSBossCharacter* BossCharacter = nullptr;
	ARSBossController* BossController = nullptr;
	if (!GetBossContext(BossCharacter, BossController))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	const FRSTelegraphPresentation Presentation = TelegraphLeadTime.MakePresentation(ImpactDelay);
	BossCharacter->GetAttackTelegraphComponent()->ShowShape(AttackShape, LockedAttackTransform, Presentation);

	if (URSCombatFunctionLibrary::IsHitCheckDebugEnabled())
	{
		UE_LOG(LogTemp, Log, TEXT("%s [Strike %d] Montage Start"), *GetName(), CurrentStrikeIndex);
	}

	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, AttackMontage, MontagePlayRate);
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleAttackMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleAttackMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleAttackMontageInterrupted);
	MontageTask->ReadyForActivation();

	if (bIsCleaningUp || State != ERSTargetedSlamState::Attacking)
	{
		return;
	}

	ImpactDelayTask = UAbilityTask_WaitDelay::WaitDelay(this, ImpactDelay);
	ImpactDelayTask->OnFinish.AddDynamic(this, &ThisClass::HandleImpactDelayFinished);
	ImpactDelayTask->ReadyForActivation();
}

void URSGameplayAbility_TargetedSlam::HandleImpactDelayFinished()
{
	ImpactDelayTask = nullptr;

	if (bIsCleaningUp)
	{
		return;
	}

	if (State != ERSTargetedSlamState::Attacking || bHasExecutedImpact)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s ignored an invalid or duplicate Impact callback during strike %d"), *GetName(), CurrentStrikeIndex);

		return;
	}

	// Callback이 중복으로 들어와도 피해를 두 번 적용하지 않도록 판정 전에 먼저 완료 처리합니다
	bHasExecutedImpact = true;

	if (URSCombatFunctionLibrary::IsHitCheckDebugEnabled())
	{
		UE_LOG(LogTemp, Log, TEXT("%s [Strike %d] Impact"), *GetName(), CurrentStrikeIndex);
	}

	ARSBossCharacter* BossCharacter = nullptr;
	ARSBossController* BossController = nullptr;
	if (!GetBossContext(BossCharacter, BossController))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	TArray<AActor*> HitTargets;
	URSCombatFunctionLibrary::FindTargetsInShape(BossCharacter, TargetChannel, AttackShape, LockedAttackTransform, HitTargets);

	const float DamageAmount = Damage.GetValueAtLevel(GetAbilityLevel(CurrentSpecHandle, CurrentActorInfo));
	for (AActor* HitTarget : HitTargets)
	{
		ApplyDamageToTarget(HitTarget, DamageEffectClass, DamageAmount);
		URSCombatFunctionLibrary::SendHitReaction(BossCharacter, HitTarget, Reaction);
	}

	TryFinishStrike();
}

void URSGameplayAbility_TargetedSlam::HandleAttackMontageCompleted()
{
	if (bIsCleaningUp)
	{
		return;
	}

	if (State != ERSTargetedSlamState::Attacking)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s received Montage Completed outside the Attacking state for strike %d"), *GetName(), CurrentStrikeIndex);
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	if (URSCombatFunctionLibrary::IsHitCheckDebugEnabled())
	{
		UE_LOG(LogTemp, Log, TEXT("%s [Strike %d] Montage Completed"), *GetName(), CurrentStrikeIndex);
	}

	bHasCompletedMontage = true;
	MontageTask = nullptr;
	TryFinishStrike();
}

void URSGameplayAbility_TargetedSlam::TryFinishStrike()
{
	if (bIsCleaningUp || State != ERSTargetedSlamState::Attacking || !bHasExecutedImpact || !bHasCompletedMontage)
	{
		return;
	}

	State = ERSTargetedSlamState::Inactive;
	++CurrentStrikeIndex;

	if (CurrentStrikeIndex >= StrikeCount)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);

		return;
	}

	BeginStrike();
}

void URSGameplayAbility_TargetedSlam::HandleAttackMontageInterrupted()
{
	if (!bIsCleaningUp)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

void URSGameplayAbility_TargetedSlam::ResetStrikeTransientState()
{
	AimTargetActor.Reset();
	AimSnapshotLocation = FVector::ZeroVector;
	LockedAttackTransform = FTransform::Identity;
	PreAimStartTime = 0.0f;
	bHasExecutedImpact = false;
	bHasCompletedMontage = false;
	ObserveFacingTask = nullptr;
	ImpactDelayTask = nullptr;
	MontageTask = nullptr;
}

bool URSGameplayAbility_TargetedSlam::GetBossContext(ARSBossCharacter*& OutBossCharacter, ARSBossController*& OutBossController) const
{
	OutBossCharacter = CurrentActorInfo ? Cast<ARSBossCharacter>(CurrentActorInfo->AvatarActor.Get()) : nullptr;
	OutBossController = OutBossCharacter ? Cast<ARSBossController>(OutBossCharacter->GetController()) : nullptr;

	return OutBossCharacter && OutBossController;
}

#if WITH_EDITOR
EDataValidationResult URSGameplayAbility_TargetedSlam::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult ValidationResult = Super::IsDataValid(Context);

	if (StrikeCount < 1 || StrikeCount > 2)
	{
		Context.AddError(FText::FromString(TEXT("Targeted Slam supports only one or two Strikes.")));
		ValidationResult = EDataValidationResult::Invalid;
	}

	if (AttackShape.Type != ERSCombatShapeType::Cone)
	{
		Context.AddError(FText::FromString(TEXT("AttackShape must use the Cone shape.")));
		ValidationResult = EDataValidationResult::Invalid;
	}
	else
	{
		FString ShapeValidationError;
		if (!AttackShape.IsDataValid(&ShapeValidationError))
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("AttackShape is invalid: %s"), *ShapeValidationError)));
			ValidationResult = EDataValidationResult::Invalid;
		}
	}

	if (!AttackMontage)
	{
		Context.AddError(FText::FromString(TEXT("AttackMontage is required.")));
		ValidationResult = EDataValidationResult::Invalid;
	}

	if (!FMath::IsFinite(ImpactDelay) || ImpactDelay <= 0.0f)
	{
		Context.AddError(FText::FromString(TEXT("ImpactDelay must be finite and greater than zero.")));
		ValidationResult = EDataValidationResult::Invalid;
	}

	FString TelegraphLeadValidationError;
	if (!TelegraphLeadTime.IsDataValid(ImpactDelay, &TelegraphLeadValidationError))
	{
		Context.AddError(FText::FromString(FString::Printf(TEXT("TelegraphLeadTime is invalid: %s"), *TelegraphLeadValidationError)));
		ValidationResult = EDataValidationResult::Invalid;
	}

	if (AttackMontage && FMath::IsFinite(ImpactDelay) && FMath::IsFinite(MontagePlayRate) && MontagePlayRate > 0.0f && ImpactDelay > AttackMontage->GetPlayLength() / MontagePlayRate)
	{
		Context.AddError(FText::FromString(TEXT("ImpactDelay must not exceed the effective AttackMontage length.")));
		ValidationResult = EDataValidationResult::Invalid;
	}

	if (!FMath::IsFinite(AimRotationSpeed) || AimRotationSpeed <= 0.0f
		|| !FMath::IsFinite(AimYawTolerance) || AimYawTolerance < 0.0f
		|| !FMath::IsFinite(TargetDriftTolerance) || TargetDriftTolerance < 0.0f
		|| !FMath::IsFinite(MaxAimDuration) || MaxAimDuration <= 0.0f
		|| !FMath::IsFinite(MontagePlayRate) || MontagePlayRate <= 0.0f)
	{
		Context.AddError(FText::FromString(TEXT("Aim and Montage timing values are outside their supported ranges.")));
		ValidationResult = EDataValidationResult::Invalid;
	}

	if (!DamageEffectClass)
	{
		Context.AddError(FText::FromString(TEXT("DamageEffectClass is required.")));
		ValidationResult = EDataValidationResult::Invalid;
	}

	if (!URSCombatFunctionLibrary::ValidateIntegerDamage(Damage, TEXT("Damage"), Context))
	{
		ValidationResult = EDataValidationResult::Invalid;
	}

	const FGameplayTagContainer& AssetTags = GetAssetTags();
	const bool bHasOneStrikeTag = AssetTags.HasTagExact(RSGameplayTags::Ability_Combat_TargetedSlam_OneStrike);
	const bool bHasTwoStrikeTag = AssetTags.HasTagExact(RSGameplayTags::Ability_Combat_TargetedSlam_TwoStrike);
	const bool bHasMatchingStrikeTag = StrikeCount == 1 ? bHasOneStrikeTag && !bHasTwoStrikeTag : bHasTwoStrikeTag && !bHasOneStrikeTag;
	if (!bHasMatchingStrikeTag)
	{
		Context.AddError(FText::FromString(TEXT("StrikeCount must exactly match the OneStrike or TwoStrike Targeted Slam leaf AssetTag.")));
		ValidationResult = EDataValidationResult::Invalid;
	}

	return ValidationResult;
}
#endif
