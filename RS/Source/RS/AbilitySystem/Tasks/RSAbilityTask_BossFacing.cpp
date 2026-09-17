#include "RSAbilityTask_BossFacing.h"

#include "Abilities/GameplayAbility.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "RSBossCharacter.h"
#include "RSBossController.h"

namespace
{
	constexpr float RSFixedWorldFocusDistance = 10000.0f;
}

FRSBossFacingRequest FRSBossFacingRequest::MakeTrackActor(AActor* TargetActor, float RotationSpeedDegreesPerSecond, float YawToleranceDegrees, float TargetDriftTolerance, float MaxDuration)
{
	FRSBossFacingRequest Request;
	Request.Mode = ERSBossFacingMode::TrackActor;
	Request.TargetActor = TargetActor;
	Request.RotationSpeedDegreesPerSecond = RotationSpeedDegreesPerSecond;
	Request.YawToleranceDegrees = YawToleranceDegrees;
	Request.TargetDriftTolerance = TargetDriftTolerance;
	Request.MaxDuration = MaxDuration;

	return Request;
}

FRSBossFacingRequest FRSBossFacingRequest::MakeFixedWorldYaw(float WorldYawDegrees, float RotationSpeedDegreesPerSecond, float YawToleranceDegrees, float MaxDuration)
{
	FRSBossFacingRequest Request;
	Request.Mode = ERSBossFacingMode::FixedWorldYaw;
	Request.WorldYawDegrees = WorldYawDegrees;
	Request.RotationSpeedDegreesPerSecond = RotationSpeedDegreesPerSecond;
	Request.YawToleranceDegrees = YawToleranceDegrees;
	Request.MaxDuration = MaxDuration;

	return Request;
}

bool FRSBossFacingRequest::IsDataValid() const
{
	if (!FMath::IsFinite(RotationSpeedDegreesPerSecond) || RotationSpeedDegreesPerSecond <= 0.0f
		|| !FMath::IsFinite(YawToleranceDegrees) || YawToleranceDegrees < 0.0f
		|| !FMath::IsFinite(MaxDuration) || MaxDuration <= 0.0f)
	{
		return false;
	}

	if (Mode == ERSBossFacingMode::TrackActor)
	{
		return TargetActor.IsValid() && FMath::IsFinite(TargetDriftTolerance) && TargetDriftTolerance >= 0.0f;
	}

	return Mode == ERSBossFacingMode::FixedWorldYaw && FMath::IsFinite(WorldYawDegrees);
}

URSAbilityTask_BossFacing::URSAbilityTask_BossFacing(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bTickingTask = true;
}

URSAbilityTask_BossFacing* URSAbilityTask_BossFacing::CreateBossFacingTask(UGameplayAbility* OwningAbility, const FRSBossFacingRequest& Request)
{
	URSAbilityTask_BossFacing* Task = NewAbilityTask<URSAbilityTask_BossFacing>(OwningAbility);
	Task->FacingRequest = Request;

	return Task;
}

void URSAbilityTask_BossFacing::Activate()
{
	Super::Activate();

	const FGameplayAbilityActorInfo* ActorInfo = Ability ? Ability->GetCurrentActorInfo() : nullptr;
	ARSBossCharacter* Character = ActorInfo ? Cast<ARSBossCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	ARSBossController* Controller = Character ? Cast<ARSBossController>(Character->GetController()) : nullptr;
	UCharacterMovementComponent* CharacterMovementComp = Character ? Character->GetCharacterMovement() : nullptr;
	if (!FacingRequest.IsDataValid() || !Character || !Controller || !CharacterMovementComp || !Character->GetWorld())
	{
		FinishFacing(ERSBossFacingResult::InvalidContext, false);

		return;
	}

	BossCharacter = Character;
	BossController = Controller;
	OriginalRotationRate = CharacterMovementComp->RotationRate;
	bOriginalUseControllerDesiredRotation = CharacterMovementComp->bUseControllerDesiredRotation;
	bHasSavedRotationState = true;
	CharacterMovementComp->RotationRate.Yaw = FacingRequest.RotationSpeedDegreesPerSecond;
	CharacterMovementComp->bUseControllerDesiredRotation = true;
	StartTimeSeconds = Character->GetWorld()->GetTimeSeconds();

	if (FacingRequest.Mode == ERSBossFacingMode::TrackActor)
	{
		AActor* TargetActor = FacingRequest.TargetActor.Get();
		if (!Controller->IsTargetActorValid(TargetActor))
		{
			FinishFacing(ERSBossFacingResult::TargetInvalid, false);

			return;
		}

		TargetSnapshotLocation = TargetActor->GetActorLocation();
		Controller->SetFocalPoint(TargetSnapshotLocation, EAIFocusPriority::Gameplay);
		bHasAppliedGameplayFocus = true;

		return;
	}

	FacingRequest.WorldYawDegrees = FRotator::NormalizeAxis(FacingRequest.WorldYawDegrees);
	UpdateFixedWorldFocalPoint();
	bHasAppliedGameplayFocus = true;
}

void URSAbilityTask_BossFacing::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);

	if (bHasFinished)
	{
		return;
	}

	if (!BossCharacter.IsValid() || !BossController.IsValid())
	{
		FinishFacing(ERSBossFacingResult::InvalidContext, false);

		return;
	}

	if (FacingRequest.Mode == ERSBossFacingMode::TrackActor)
	{
		TickActorTracking();

		return;
	}

	TickFixedWorldYaw();
}

void URSAbilityTask_BossFacing::OnDestroy(bool bInOwnerFinished)
{
	RestoreFacingState();

	Super::OnDestroy(bInOwnerFinished);
}

void URSAbilityTask_BossFacing::TickActorTracking()
{
	ARSBossCharacter* Character = BossCharacter.Get();
	ARSBossController* Controller = BossController.Get();
	AActor* TargetActor = FacingRequest.TargetActor.Get();
	if (!Character || !Controller || !Controller->IsTargetActorValid(TargetActor))
	{
		FinishFacing(ERSBossFacingResult::TargetInvalid, false);

		return;
	}

	const FVector FacingOffset(TargetSnapshotLocation.X - Character->GetActorLocation().X, TargetSnapshotLocation.Y - Character->GetActorLocation().Y, 0.0f);
	if (FacingOffset.IsNearlyZero())
	{
		FinishFacing(ERSBossFacingResult::NoDirection, true);

		return;
	}

	const double ElapsedTime = Character->GetWorld()->GetTimeSeconds() - StartTimeSeconds;
	if (ElapsedTime >= FacingRequest.MaxDuration)
	{
		FinishFacing(ERSBossFacingResult::TimedOut, true);

		return;
	}

	bool bIsWithinYawTolerance = false;
	float YawErrorDegrees = 0.0f;
	CalculateYawState(Character->GetActorRotation().Yaw, FacingOffset.Rotation().Yaw, FacingRequest.YawToleranceDegrees, bIsWithinYawTolerance, YawErrorDegrees);
	if (!bIsWithinYawTolerance)
	{
		return;
	}

	if (FVector::DistSquared2D(TargetActor->GetActorLocation(), TargetSnapshotLocation) > FMath::Square(FacingRequest.TargetDriftTolerance))
	{
		TargetSnapshotLocation = TargetActor->GetActorLocation();
		Controller->SetFocalPoint(TargetSnapshotLocation, EAIFocusPriority::Gameplay);

		return;
	}

	FinishFacing(ERSBossFacingResult::Aligned, true);
}

void URSAbilityTask_BossFacing::TickFixedWorldYaw()
{
	ARSBossCharacter* Character = BossCharacter.Get();
	if (!Character)
	{
		FinishFacing(ERSBossFacingResult::InvalidContext, false);

		return;
	}

	UpdateFixedWorldFocalPoint();

	bool bIsWithinYawTolerance = false;
	float YawErrorDegrees = 0.0f;
	CalculateYawState(Character->GetActorRotation().Yaw, FacingRequest.WorldYawDegrees, FacingRequest.YawToleranceDegrees, bIsWithinYawTolerance, YawErrorDegrees);
	const double ElapsedTime = Character->GetWorld()->GetTimeSeconds() - StartTimeSeconds;
	if (!bIsWithinYawTolerance && ElapsedTime < FacingRequest.MaxDuration)
	{
		return;
	}

	ApplyExactWorldYaw();
	FinishFacing(bIsWithinYawTolerance ? ERSBossFacingResult::Aligned : ERSBossFacingResult::TimedOut, true);
}

void URSAbilityTask_BossFacing::UpdateFixedWorldFocalPoint()
{
	ARSBossCharacter* Character = BossCharacter.Get();
	ARSBossController* Controller = BossController.Get();
	if (!Character || !Controller)
	{
		return;
	}

	const FVector FacingDirection = FRotator(0.0f, FacingRequest.WorldYawDegrees, 0.0f).Vector();
	Controller->SetFocalPoint(Character->GetActorLocation() + FacingDirection * RSFixedWorldFocusDistance, EAIFocusPriority::Gameplay);
}

void URSAbilityTask_BossFacing::ApplyExactWorldYaw()
{
	ARSBossCharacter* Character = BossCharacter.Get();
	ARSBossController* Controller = BossController.Get();
	if (!Character || !Controller)
	{
		return;
	}

	Controller->ClearFocus(EAIFocusPriority::Gameplay);
	bHasAppliedGameplayFocus = false;
	Controller->SetControlRotation(MakeExactYawRotation(Controller->GetControlRotation(), FacingRequest.WorldYawDegrees));
	Character->SetActorRotation(MakeExactYawRotation(Character->GetActorRotation(), FacingRequest.WorldYawDegrees));
}

void URSAbilityTask_BossFacing::LockCompletedFacing()
{
	ARSBossCharacter* Character = BossCharacter.Get();
	ARSBossController* Controller = BossController.Get();
	if (Controller && bHasAppliedGameplayFocus)
	{
		Controller->ClearFocus(EAIFocusPriority::Gameplay);
		bHasAppliedGameplayFocus = false;
	}

	if (Character)
	{
		if (UCharacterMovementComponent* CharacterMovementComp = Character->GetCharacterMovement())
		{
			CharacterMovementComp->bUseControllerDesiredRotation = false;
		}
	}
}

void URSAbilityTask_BossFacing::RestoreFacingState()
{
	ARSBossCharacter* Character = BossCharacter.Get();
	ARSBossController* Controller = BossController.Get();
	if (Controller && bHasAppliedGameplayFocus)
	{
		Controller->ClearFocus(EAIFocusPriority::Gameplay);
		bHasAppliedGameplayFocus = false;
	}

	if (Character && bHasSavedRotationState)
	{
		if (UCharacterMovementComponent* CharacterMovementComp = Character->GetCharacterMovement())
		{
			CharacterMovementComp->RotationRate = OriginalRotationRate;
			CharacterMovementComp->bUseControllerDesiredRotation = bOriginalUseControllerDesiredRotation;
		}
	}

	bHasSavedRotationState = false;
}

void URSAbilityTask_BossFacing::FinishFacing(ERSBossFacingResult Result, bool bShouldLockFacing)
{
	if (bHasFinished)
	{
		return;
	}

	bHasFinished = true;
	bTickingTask = false;
	if (bShouldLockFacing)
	{
		LockCompletedFacing();
	}
	else
	{
		RestoreFacingState();
	}

	const ARSBossCharacter* Character = BossCharacter.Get();
	const float FinalYawDegrees = Character ? Character->GetActorRotation().Yaw : 0.0f;
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnFacingFinished.Broadcast(Result, FinalYawDegrees);
	}

	// Callback이 Task나 Ability를 끝낼 수 있으므로 Broadcast 뒤에는 상태를 다시 읽거나 변경하지 않습니다
}

void URSAbilityTask_BossFacing::CalculateYawState(float CurrentYawDegrees, float TargetYawDegrees, float ToleranceDegrees, bool& bOutIsWithinYawTolerance, float& OutYawErrorDegrees)
{
	OutYawErrorDegrees = FMath::FindDeltaAngleDegrees(CurrentYawDegrees, TargetYawDegrees);
	bOutIsWithinYawTolerance = FMath::Abs(OutYawErrorDegrees) <= ToleranceDegrees;
}

FRotator URSAbilityTask_BossFacing::MakeExactYawRotation(const FRotator& CurrentRotation, float TargetYawDegrees)
{
	return FRotator(CurrentRotation.Pitch, FRotator::NormalizeAxis(TargetYawDegrees), CurrentRotation.Roll);
}
