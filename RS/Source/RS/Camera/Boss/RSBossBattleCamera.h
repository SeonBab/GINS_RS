#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraActor.h"
#include "RSBossBattleCamera.generated.h"

class APawn;
class USceneComponent;

/**
 * 보스전 카메라가 구도를 계산하는 데 필요한 최소 입력입니다
 * 전투 진행 상태나 Encounter 타입을 담지 않아 카메라가 보스전 도메인을 알지 않게 합니다
 */
struct FRSBossCameraContext
{
	/** 카메라가 공전할 중심이 되는 공간 기준 Component입니다 */
	TWeakObjectPtr<USceneComponent> PivotComponent;

	/** 방위각 계산의 기준이 되는 로컬 플레이어 Pawn입니다 */
	TWeakObjectPtr<APawn> TargetPlayer;
};

/** 전달받은 Pivot을 중심으로 로컬 플레이어의 보스전 구도를 계산하는 CameraActor입니다 */
UCLASS()
class RS_API ARSBossBattleCamera : public ACameraActor
{
	GENERATED_BODY()

public:
	/** 카메라가 로컬에서만 갱신되도록 Tick 기본값을 구성합니다 */
	ARSBossBattleCamera();

	/** 전달받은 구도 입력으로 카메라 Transform을 갱신합니다 */
	virtual void Tick(float DeltaTime) override;

	/**
	 * 구도 입력을 받아 카메라 갱신을 시작하고 현재 플레이어 방향으로 즉시 Snap합니다
	 * 카메라의 활성 여부는 스스로 판단하지 않으므로 소유자만 이 함수를 호출합니다
	 */
	void ActivateCamera(const FRSBossCameraContext& InCameraContext);

	/** 카메라 갱신을 중단하고 구도 입력과 누적된 방위각을 정리합니다 */
	void DeactivateCamera();

	/** Pawn이 교체되어도 같은 카메라가 새 플레이어를 기준으로 공전하게 합니다 */
	void SetTargetPlayer(APawn* InTargetPlayer);

	/** 캐릭터 카메라와 보스전 카메라 사이의 전환 시간을 반환합니다 */
	float GetViewTargetBlendTime() const { return ViewTargetBlendTime; }

private:
	/** 현재 구도 입력으로 카메라 위치와 회전을 계산합니다 */
	void UpdateCamera(float DeltaTime, bool bSnapToTarget);

	/** Pivot 기준 플레이어 방위각을 최대 회전 속도 안에서 갱신합니다 */
	void UpdateOrbitYaw(const FVector& PivotLocation, const FVector& TargetPlayerLocation, float DeltaTime, bool bSnapToTarget);

private:
	/** 소유자가 전달한 Pivot과 추적 대상입니다 */
	FRSBossCameraContext CameraContext;

	/** Pivot에서 카메라까지 유지할 수평 거리입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Camera", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm"))
	float OrbitRadius = 1800.0f;

	/** Pivot보다 카메라를 높게 배치할 거리입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Camera", meta = (AllowPrivateAccess = "true", Units = "cm"))
	float CameraHeight = 1400.0f;

	/** 플레이어의 Pivot 기준 방위각에 더할 구도 Offset입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Camera", meta = (AllowPrivateAccess = "true", Units = "deg"))
	float OrbitYawOffset = 0.0f;

	/** 정확한 바닥 Pivot 대신 카메라가 바라볼 높이입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Camera", meta = (AllowPrivateAccess = "true", Units = "cm"))
	float LookAtHeight = 150.0f;

	/** 플레이어의 새 방위각을 따라갈 최대 회전 속도입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Camera", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "deg/s"))
	float MaximumOrbitRotationSpeed = 180.0f;

	/** ViewTarget이 전환될 때 사용할 Blend 시간입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Camera", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "s"))
	float ViewTargetBlendTime = 0.6f;

	/** Pivot과 플레이어가 겹쳐도 마지막 유효 구도를 유지할 현재 방위각입니다 */
	float CurrentOrbitYaw = 0.0f;

	/** 최초 갱신에서 현재 방위각을 목표값으로 초기화했는지 나타냅니다 */
	bool bHasCurrentOrbitYaw = false;
};
