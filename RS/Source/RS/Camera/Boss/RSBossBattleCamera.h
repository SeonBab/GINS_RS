#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraActor.h"
#include "RSBossCameraTypes.h"
#include "RSBossBattleCamera.generated.h"

class APawn;
class USceneComponent;

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
	float GetViewTargetBlendTime() const { return CameraContext.Settings.ViewTargetBlendTime; }

private:
	/** 현재 구도 입력으로 카메라 위치와 회전을 계산합니다 */
	void UpdateCamera(float DeltaTime, bool bSnapToTarget);

	/** Pivot 기준 플레이어 방위각을 최대 회전 속도 안에서 갱신합니다 */
	void UpdateOrbitYaw(const FVector& PivotLocation, const FVector& TargetPlayerLocation, float DeltaTime, bool bSnapToTarget);

	/** 플레이어의 Pivot 기준 거리를 반영하고 제한을 적용한 궤도 반지름을 반환합니다 */
	float CalculateOrbitRadius(const FVector& PivotLocation, const FVector& TargetPlayerLocation) const;

	/** 화면 구성에 관여하는 카메라 설정을 추적 대상 Pawn의 카메라와 같게 맞춥니다 */
	void ApplyTargetPlayerCameraSettings();

private:
	/** 소유자가 전달한 Pivot과 추적 대상입니다 */
	FRSBossCameraContext CameraContext;

	/** Pivot과 플레이어가 겹쳐도 마지막 유효 구도를 유지할 현재 방위각입니다 */
	float CurrentOrbitYaw = 0.0f;

	/** 최초 갱신에서 현재 방위각을 목표값으로 초기화했는지 나타냅니다 */
	bool bHasCurrentOrbitYaw = false;
};
