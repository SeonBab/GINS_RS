#include "RSAbilityTask_ObserveFacing.h"

#include "Abilities/GameplayAbility.h"

URSAbilityTask_ObserveFacing::URSAbilityTask_ObserveFacing(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bTickingTask = true;
}

URSAbilityTask_ObserveFacing* URSAbilityTask_ObserveFacing::ObserveFacing(UGameplayAbility* OwningAbility, const FVector& FacingLocation, float YawToleranceDegrees)
{
	URSAbilityTask_ObserveFacing* Task = NewAbilityTask<URSAbilityTask_ObserveFacing>(OwningAbility);
	const FGameplayAbilityActorInfo* ActorInfo = OwningAbility ? OwningAbility->GetCurrentActorInfo() : nullptr;
	Task->CachedAvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	Task->FacingLocation = FacingLocation;
	Task->YawToleranceDegrees = YawToleranceDegrees;

	return Task;
}

void URSAbilityTask_ObserveFacing::SetFacingLocation(const FVector& NewFacingLocation)
{
	FacingLocation = NewFacingLocation;
}

void URSAbilityTask_ObserveFacing::Activate()
{
	Super::Activate();

	if (!CachedAvatarActor.IsValid() || YawToleranceDegrees < 0.0f)
	{
		EndTask();
	}
}

void URSAbilityTask_ObserveFacing::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);

	const AActor* AvatarActor = CachedAvatarActor.Get();
	if (!AvatarActor)
	{
		EndTask();

		return;
	}

	bool bHasFacingDirection = false;
	bool bIsWithinYawTolerance = false;
	float YawErrorDegrees = 0.0f;
	CalculateFacingState(*AvatarActor, FacingLocation, YawToleranceDegrees, bHasFacingDirection, bIsWithinYawTolerance, YawErrorDegrees);

	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnFacingUpdated.Broadcast(bHasFacingDirection, bIsWithinYawTolerance, YawErrorDegrees);
	}

	// Callback이 Task나 Ability를 끝낼 수 있으므로 Broadcast 뒤에는 상태를 다시 읽거나 변경하지 않습니다
}

void URSAbilityTask_ObserveFacing::CalculateFacingState(const AActor& AvatarActor, const FVector& TargetLocation, float ToleranceDegrees, bool& bOutHasFacingDirection, bool& bOutIsWithinYawTolerance, float& OutYawErrorDegrees)
{
	const FVector FacingOffset = FVector(TargetLocation.X - AvatarActor.GetActorLocation().X, TargetLocation.Y - AvatarActor.GetActorLocation().Y, 0.0f);
	bOutHasFacingDirection = !FacingOffset.IsNearlyZero();
	bOutIsWithinYawTolerance = false;
	OutYawErrorDegrees = 0.0f;

	if (!bOutHasFacingDirection)
	{
		return;
	}

	const float CurrentYawDegrees = AvatarActor.GetActorRotation().Yaw;
	const float TargetYawDegrees = FacingOffset.Rotation().Yaw;
	OutYawErrorDegrees = FMath::FindDeltaAngleDegrees(CurrentYawDegrees, TargetYawDegrees);
	bOutIsWithinYawTolerance = FMath::Abs(OutYawErrorDegrees) <= ToleranceDegrees;
}
