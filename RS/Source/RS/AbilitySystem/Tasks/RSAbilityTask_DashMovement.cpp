// Fill out your copyright notice in the Description page of Project Settings.

#include "RSAbilityTask_DashMovement.h"

#include "GameFramework/CharacterMovementComponent.h"

URSAbilityTask_DashMovement::URSAbilityTask_DashMovement(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bTickingTask = true;
}

URSAbilityTask_DashMovement* URSAbilityTask_DashMovement::CreateDashMovement(UGameplayAbility* OwningAbility, UCharacterMovementComponent* MovementComponent, const FVector& Direction, float Distance, float Duration, const FRichCurve& ProgressCurve)
{
	URSAbilityTask_DashMovement* Task = NewAbilityTask<URSAbilityTask_DashMovement>(OwningAbility);
	Task->CachedMovementComponent = MovementComponent;
	Task->DashDirection = Direction.GetSafeNormal2D();
	Task->DashDistance = Distance;
	Task->DashDuration = Duration;
	Task->DistanceProgressCurve = ProgressCurve;

	return Task;
}

void URSAbilityTask_DashMovement::Activate()
{
	Super::Activate();

	UCharacterMovementComponent* MovementComponent = CachedMovementComponent.Get();
	if (!MovementComponent || !MovementComponent->UpdatedComponent || DashDirection.IsNearlyZero() || DashDistance <= 0.0f || DashDuration <= 0.0f || DistanceProgressCurve.GetNumKeys() < 2)
	{
		EndTask();
	}
}

void URSAbilityTask_DashMovement::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);

	UCharacterMovementComponent* MovementComponent = CachedMovementComponent.Get();
	if (!MovementComponent || !MovementComponent->UpdatedComponent)
	{
		EndTask();

		return;
	}

	ElapsedTime = FMath::Min(ElapsedTime + DeltaTime, DashDuration);

	const float NormalizedTime = ElapsedTime / DashDuration;
	const float Progress = FMath::Clamp(DistanceProgressCurve.Eval(NormalizedTime), 0.0f, 1.0f);
	const float DesiredDistance = DashDistance * Progress;
	const float DeltaDistance = FMath::Max(DesiredDistance - PreviousDesiredDistance, 0.0f);
	PreviousDesiredDistance = FMath::Max(PreviousDesiredDistance, DesiredDistance);

	if (DeltaDistance > UE_KINDA_SMALL_NUMBER)
	{
		const FVector MovementDelta = DashDirection * DeltaDistance;
		FHitResult Hit;
		MovementComponent->SafeMoveUpdatedComponent(MovementDelta, MovementComponent->UpdatedComponent->GetComponentQuat(), true, Hit);

		if (Hit.IsValidBlockingHit())
		{
			// 막힌 거리를 다음 프레임에 보충하지 않고 남은 이동량만 표면 방향으로 처리합니다
			UMovementComponent* BaseMovementComponent = MovementComponent;
			BaseMovementComponent->SlideAlongSurface(MovementDelta, 1.0f - Hit.Time, Hit.Normal, Hit, true);
		}
	}

	if (ElapsedTime >= DashDuration)
	{
		EndTask();
	}
}
