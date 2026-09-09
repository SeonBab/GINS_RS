// Fill out your copyright notice in the Description page of Project Settings.

#include "RSAbilityTask_CurveMovement.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

URSAbilityTask_CurveMovement::URSAbilityTask_CurveMovement(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bTickingTask = true;
}

URSAbilityTask_CurveMovement* URSAbilityTask_CurveMovement::CreateCurveMovement(UGameplayAbility* OwningAbility, UCharacterMovementComponent* MovementComponent, const FVector& Direction, float Distance, float Duration, const FRichCurve& ProgressCurve)
{
	URSAbilityTask_CurveMovement* Task = NewAbilityTask<URSAbilityTask_CurveMovement>(OwningAbility);
	Task->CachedMovementComponent = MovementComponent;
	Task->MovementDirection = Direction.GetSafeNormal2D();
	Task->MovementDistance = FMath::IsFinite(Distance) ? FMath::Max(Distance, 0.0f) : 0.0f;
	Task->MovementDuration = Duration;
	Task->DistanceProgressCurve = ProgressCurve;

	return Task;
}

void URSAbilityTask_CurveMovement::SetMeshArc(USkeletalMeshComponent* MeshComponent, float Height)
{
	if (!MeshComponent || !FMath::IsFinite(Height) || Height <= 0.0f)
	{
		return;
	}

	CachedMeshComponent = MeshComponent;
	MeshBaseRelativeLocation = MeshComponent->GetRelativeLocation();
	ArcHeight = Height;
}

void URSAbilityTask_CurveMovement::Activate()
{
	Super::Activate();

	if (!FMath::IsFinite(MovementDuration) || MovementDuration <= 0.0f)
	{
		EndTask();

		return;
	}

	if (HasHorizontalMovement())
	{
		UCharacterMovementComponent* MovementComponent = CachedMovementComponent.Get();
		if (!MovementComponent || !MovementComponent->UpdatedComponent || DistanceProgressCurve.GetNumKeys() < 2)
		{
			EndTask();

			return;
		}
	}
}

void URSAbilityTask_CurveMovement::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);

	ElapsedTime = FMath::Min(ElapsedTime + DeltaTime, MovementDuration);

	const float NormalizedTime = ElapsedTime / MovementDuration;
	if (HasHorizontalMovement())
	{
		UCharacterMovementComponent* MovementComponent = CachedMovementComponent.Get();
		if (!MovementComponent || !MovementComponent->UpdatedComponent)
		{
			EndTask();

			return;
		}

		const float Progress = FMath::Clamp(DistanceProgressCurve.Eval(NormalizedTime), 0.0f, 1.0f);
		const float DesiredDistance = MovementDistance * Progress;
		const float DeltaDistance = FMath::Max(DesiredDistance - PreviousDesiredDistance, 0.0f);
		PreviousDesiredDistance = FMath::Max(PreviousDesiredDistance, DesiredDistance);

		if (DeltaDistance > UE_KINDA_SMALL_NUMBER)
		{
			const FVector MovementDelta = MovementDirection * DeltaDistance;
			FHitResult Hit;
			MovementComponent->SafeMoveUpdatedComponent(MovementDelta, MovementComponent->UpdatedComponent->GetComponentQuat(), true, Hit);

			if (Hit.IsValidBlockingHit())
			{
				// 막힌 거리를 다음 프레임에 보충하지 않고 남은 이동량만 표면 방향으로 처리합니다
				UMovementComponent* BaseMovementComponent = MovementComponent;
				BaseMovementComponent->SlideAlongSurface(MovementDelta, 1.0f - Hit.Time, Hit.Normal, Hit, true);
			}
		}
	}

	if (USkeletalMeshComponent* MeshComponent = CachedMeshComponent.Get())
	{
		// 정규화 시간 0과 1에서 0이 되는 아치라 시작과 착지 높이가 원래 위치와 정확히 같습니다
		const float ArcOffset = ArcHeight * FMath::Sin(UE_PI * NormalizedTime);
		MeshComponent->SetRelativeLocation(MeshBaseRelativeLocation + FVector(0.0f, 0.0f, ArcOffset));
	}

	if (ElapsedTime >= MovementDuration)
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnCompleted.Broadcast();
		}

		EndTask();
	}
}

void URSAbilityTask_CurveMovement::OnDestroy(bool bInOwnerFinished)
{
	// 어빌리티가 중간에 취소되어도 Mesh가 떠 있는 상태로 남지 않게 합니다
	RestoreMeshLocation();

	Super::OnDestroy(bInOwnerFinished);
}

bool URSAbilityTask_CurveMovement::HasHorizontalMovement() const
{
	return !MovementDirection.IsNearlyZero() && MovementDistance > 0.0f;
}

void URSAbilityTask_CurveMovement::RestoreMeshLocation()
{
	USkeletalMeshComponent* MeshComponent = CachedMeshComponent.Get();
	if (!MeshComponent)
	{
		return;
	}

	MeshComponent->SetRelativeLocation(MeshBaseRelativeLocation);
	CachedMeshComponent = nullptr;
}
