#include "RSBossBattleCamera.h"

#include "Components/SceneComponent.h"
#include "GameFramework/Pawn.h"

ARSBossBattleCamera::ARSBossBattleCamera()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	bReplicates = false;
}

void ARSBossBattleCamera::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateCamera(DeltaTime, false);
}

void ARSBossBattleCamera::ActivateCamera(const FRSBossCameraContext& InCameraContext)
{
	CameraContext = InCameraContext;
	bHasCurrentOrbitYaw = false;

	// Blend가 이전 구도에서 시작되지 않도록 ViewTarget 전환 전에 최종 구도를 배치합니다
	UpdateCamera(0.0f, true);
	SetActorTickEnabled(true);
}

void ARSBossBattleCamera::DeactivateCamera()
{
	SetActorTickEnabled(false);
	CameraContext = FRSBossCameraContext();
	bHasCurrentOrbitYaw = false;
}

void ARSBossBattleCamera::SetTargetPlayer(APawn* InTargetPlayer)
{
	CameraContext.TargetPlayer = InTargetPlayer;
}

void ARSBossBattleCamera::UpdateCamera(float DeltaTime, bool bSnapToTarget)
{
	USceneComponent* PivotComponent = CameraContext.PivotComponent.Get();
	APawn* TargetPlayer = CameraContext.TargetPlayer.Get();

	// 참조가 일시적으로 무효인 것은 이번 Frame을 건너뛸 사유일 뿐이며 활성 여부는 소유자가 결정합니다
	if (!IsValid(PivotComponent) || !IsValid(TargetPlayer))
	{
		return;
	}

	const FVector PivotLocation = PivotComponent->GetComponentLocation();
	UpdateOrbitYaw(PivotLocation, TargetPlayer->GetActorLocation(), DeltaTime, bSnapToTarget);

	// 위치를 따로 보간하면 이동 중에 궤도 안쪽을 가로질러 반지름이 흔들리므로 방위각에서 직접 계산합니다
	const FVector OrbitDirection = FRotator(0.0f, CurrentOrbitYaw, 0.0f).Vector();
	const FVector CameraLocation = PivotLocation + OrbitDirection * OrbitRadius + FVector::UpVector * CameraHeight;
	const FVector LookAtLocation = PivotLocation + FVector::UpVector * LookAtHeight;

	SetActorLocationAndRotation(CameraLocation, (LookAtLocation - CameraLocation).Rotation());
}

void ARSBossBattleCamera::UpdateOrbitYaw(const FVector& PivotLocation, const FVector& TargetPlayerLocation, float DeltaTime, bool bSnapToTarget)
{
	FVector PlayerDirection = TargetPlayerLocation - PivotLocation;
	PlayerDirection.Z = 0.0f;

	if (!PlayerDirection.Normalize())
	{
		// Pivot과 플레이어가 겹치면 방향을 만들 수 없으므로 유효한 방위각이 생길 때까지 기본값을 사용합니다
		if (!bHasCurrentOrbitYaw)
		{
			CurrentOrbitYaw = FMath::UnwindDegrees(OrbitYawOffset);
			bHasCurrentOrbitYaw = true;
		}

		return;
	}

	const float TargetOrbitYaw = FMath::UnwindDegrees(PlayerDirection.Rotation().Yaw + OrbitYawOffset);

	if (!bHasCurrentOrbitYaw || bSnapToTarget)
	{
		CurrentOrbitYaw = TargetOrbitYaw;
		bHasCurrentOrbitYaw = true;

		return;
	}

	// 각도 차이를 -180~180도로 제한하여 경계에서 반대 방향으로 크게 회전하지 않게 합니다
	const float DeltaYaw = FMath::FindDeltaAngleDegrees(CurrentOrbitYaw, TargetOrbitYaw);
	const float MaximumYawStep = MaximumOrbitRotationSpeed * DeltaTime;
	CurrentOrbitYaw = FMath::UnwindDegrees(CurrentOrbitYaw + FMath::Clamp(DeltaYaw, -MaximumYawStep, MaximumYawStep));
}
