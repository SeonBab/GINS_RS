#include "RSPlayerCameraComponent.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "RSBossBattleCamera.h"
#include "RSPlayerController.h"

URSPlayerCameraComponent::URSPlayerCameraComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	BossBattleCameraClass = ARSBossBattleCamera::StaticClass();
}

void URSPlayerCameraComponent::BeginPlay()
{
	Super::BeginPlay();

	// 카메라는 로컬 표현이므로 서버가 대리하는 원격 플레이어에서는 어떤 CameraActor도 만들지 않습니다
	if (!IsLocalPlayerCamera())
	{
		return;
	}

	// Pawn 교체는 카메라 상태와 무관하게 발생하므로 Controller의 Possess 변경을 직접 구독합니다
	if (ARSPlayerController* PlayerController = GetPlayerController())
	{
		PlayerController->OnPossessedPawnChanged.AddUniqueDynamic(this, &ThisClass::HandlePossessedPawnChanged);
	}
}

void URSPlayerCameraComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ARSPlayerController* PlayerController = GetPlayerController())
	{
		PlayerController->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::HandlePossessedPawnChanged);
	}

	// 소유자가 파괴되어도 소유한 Actor는 함께 제거되지 않으므로 직접 생성한 CameraActor를 정리합니다
	if (IsValid(BossBattleCamera))
	{
		BossBattleCamera->Destroy();
	}

	BossBattleCamera = nullptr;
	CurrentCameraState = ERSPlayerCameraState::Player;
	bPendingReturnToPlayer = false;

	Super::EndPlay(EndPlayReason);
}

void URSPlayerCameraComponent::ActivateBossCamera(USceneComponent* PivotComponent)
{
	if (!IsLocalPlayerCamera() || !IsValid(PivotComponent))
	{
		return;
	}

	ARSPlayerController* PlayerController = GetPlayerController();
	ARSBossBattleCamera* BattleCamera = GetOrSpawnBossBattleCamera();
	if (!PlayerController || !IsValid(BattleCamera))
	{
		return;
	}

	CurrentCameraState = ERSPlayerCameraState::BossOrbit;
	bPendingReturnToPlayer = false;

	FRSBossCameraContext CameraContext;
	CameraContext.PivotComponent = PivotComponent;
	CameraContext.TargetPlayer = PlayerController->GetPawn();

	BattleCamera->ActivateCamera(CameraContext);
	PlayerController->SetViewTargetWithBlend(BattleCamera, BattleCamera->GetViewTargetBlendTime(), VTBlend_Cubic, 2.0f, true);
}

void URSPlayerCameraComponent::DeactivateBossCamera()
{
	if (CurrentCameraState != ERSPlayerCameraState::BossOrbit)
	{
		return;
	}

	CurrentCameraState = ERSPlayerCameraState::Player;

	ARSPlayerController* PlayerController = GetPlayerController();
	APawn* CurrentPawn = PlayerController ? PlayerController->GetPawn() : nullptr;

	if (!IsValid(CurrentPawn))
	{
		// 복귀할 Pawn이 없으면 마지막 구도를 유지한 채 새 Pawn이 생길 때 ViewTarget을 되돌립니다
		bPendingReturnToPlayer = true;
		return;
	}

	ReturnViewTargetToPawn(CurrentPawn);
}

ARSPlayerController* URSPlayerCameraComponent::GetPlayerController() const
{
	return Cast<ARSPlayerController>(GetOwner());
}

bool URSPlayerCameraComponent::IsLocalPlayerCamera() const
{
	const ARSPlayerController* PlayerController = GetPlayerController();

	return PlayerController && PlayerController->IsLocalController();
}

ARSBossBattleCamera* URSPlayerCameraComponent::GetOrSpawnBossBattleCamera()
{
	if (IsValid(BossBattleCamera))
	{
		return BossBattleCamera;
	}

	UWorld* World = GetWorld();
	ARSPlayerController* PlayerController = GetPlayerController();
	if (!World || !PlayerController || !BossBattleCameraClass)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = PlayerController;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	BossBattleCamera = World->SpawnActor<ARSBossBattleCamera>(BossBattleCameraClass, FTransform::Identity, SpawnParameters);

	return BossBattleCamera;
}

void URSPlayerCameraComponent::ReturnViewTargetToPawn(APawn* NewPawn)
{
	ARSPlayerController* PlayerController = GetPlayerController();
	if (!PlayerController || !IsValid(NewPawn))
	{
		return;
	}

	const float BlendTime = IsValid(BossBattleCamera) ? BossBattleCamera->GetViewTargetBlendTime() : 0.0f;
	PlayerController->SetViewTargetWithBlend(NewPawn, BlendTime, VTBlend_Cubic, 2.0f, true);

	if (IsValid(BossBattleCamera))
	{
		BossBattleCamera->DeactivateCamera();
	}

	bPendingReturnToPlayer = false;
}

void URSPlayerCameraComponent::HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	if (!IsLocalPlayerCamera())
	{
		return;
	}

	// Pawn이 없는 동안 보스전이 끝났다면 새 Pawn이 생긴 지금 실제 ViewTarget 복귀를 마칩니다
	if (bPendingReturnToPlayer)
	{
		ReturnViewTargetToPawn(NewPawn);
		return;
	}

	if (CurrentCameraState != ERSPlayerCameraState::BossOrbit || !IsValid(BossBattleCamera))
	{
		return;
	}

	// 보스전이 진행 중이면 추적 대상만 갱신하여 Orbit 갱신이 그대로 이어지게 합니다
	BossBattleCamera->SetTargetPlayer(NewPawn);
}
