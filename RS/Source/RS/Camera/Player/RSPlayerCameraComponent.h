#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RSPlayerCameraComponent.generated.h"

class APawn;
class ARSBossBattleCamera;
class ARSPlayerController;
class USceneComponent;

/** 로컬 플레이어가 현재 사용하는 카메라 종류입니다 */
UENUM()
enum class ERSPlayerCameraState : uint8
{
	/** Pawn에 부착된 기본 카메라를 사용하는 상태입니다 */
	Player,

	/** 전달받은 Pivot을 중심으로 공전하는 보스전 카메라를 사용하는 상태입니다 */
	BossOrbit
};

/**
 * 로컬 플레이어의 카메라 상태와 ViewTarget 전환, 전투 CameraActor의 수명을 관리합니다
 * 보스전 도메인을 알지 않으며 Pivot처럼 중립적인 입력만 외부에서 전달받습니다
 */
UCLASS()
class RS_API URSPlayerCameraComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** 보스전 CameraActor 클래스의 기본값을 구성합니다 */
	URSPlayerCameraComponent();

protected:
	/** 로컬 플레이어의 Possess 변경을 구독합니다 */
	virtual void BeginPlay() override;

	/** 구독을 해제하고 직접 생성한 CameraActor를 정리합니다 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	/** 전달받은 Pivot을 중심으로 보스전 카메라를 활성화하고 ViewTarget을 전환합니다 */
	void ActivateBossCamera(USceneComponent* PivotComponent);

	/** 보스전 카메라를 끝내고 현재 Pawn의 카메라로 복귀합니다 */
	void DeactivateBossCamera();

private:
	/** 이 컴포넌트를 소유한 PlayerController를 반환합니다 */
	ARSPlayerController* GetPlayerController() const;

	/** 이 컴포넌트가 로컬 플레이어의 카메라를 관리해야 하는지 반환합니다 */
	bool IsLocalPlayerCamera() const;

	/** 보스전 카메라를 필요한 시점에 한 번만 생성하고 이후에는 재사용합니다 */
	ARSBossBattleCamera* GetOrSpawnBossBattleCamera();

	/** 전달받은 Pawn으로 ViewTarget을 되돌리고 보스전 카메라 갱신을 중단합니다 */
	void ReturnViewTargetToPawn(APawn* NewPawn);

	/** Possess 대상이 바뀌면 추적 대상과 보류 중인 카메라 복귀를 처리합니다 */
	UFUNCTION()
	void HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

private:
	/** 로컬 플레이어마다 생성할 보스전 CameraActor 클래스입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Camera", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<ARSBossBattleCamera> BossBattleCameraClass;

	/** 재사용을 위해 보관하는 로컬 보스전 CameraActor입니다 */
	UPROPERTY(Transient)
	TObjectPtr<ARSBossBattleCamera> BossBattleCamera;

	/** 현재 사용 중인 카메라 종류입니다 */
	ERSPlayerCameraState CurrentCameraState = ERSPlayerCameraState::Player;

	/** 복귀할 Pawn이 없어 미뤄둔 플레이어 카메라 복귀가 남아 있는지 나타냅니다 */
	bool bPendingReturnToPlayer = false;
};
