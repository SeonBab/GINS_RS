#pragma once

#include "CoreMinimal.h"
#include "RSBossCameraTypes.generated.h"

class APawn;
class USceneComponent;

/** 보스전 Orbit Camera의 콘텐츠별 조정값입니다 */
USTRUCT(BlueprintType)
struct RS_API FRSBossCameraSettings
{
	GENERATED_BODY()

	/** Pivot에서 카메라까지 유지할 수평 거리입니다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RS|Camera", meta = (ClampMin = "0.0", Units = "cm"))
	float OrbitRadius = 1800.0f;

	/** Pivot보다 카메라를 높게 배치할 거리입니다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RS|Camera", meta = (Units = "cm"))
	float CameraHeight = 1400.0f;

	/** 플레이어의 Pivot 기준 방위각에 더할 구도 Offset입니다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RS|Camera", meta = (Units = "deg"))
	float OrbitYawOffset = 0.0f;

	/** 정확한 바닥 Pivot 대신 카메라가 바라볼 높이입니다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RS|Camera", meta = (Units = "cm"))
	float LookAtHeight = 150.0f;

	/** 플레이어의 Pivot 기준 거리 중 카메라가 따라서 물러날 비율입니다. 0이면 고정 반지름으로 동작합니다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RS|Camera", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PivotFollowRatio = 0.3f;

	/** 캐릭터가 화면을 채우지 않도록 카메라와 플레이어 사이에 유지할 최소 수평 거리입니다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RS|Camera", meta = (ClampMin = "0.0", Units = "cm"))
	float MinCameraToPlayerDistance = 1000.0f;

	/** 카메라가 전투 공간 밖으로 밀려나가지 않도록 Pivot에서 벌어질 수 있는 최대 수평 거리입니다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RS|Camera", meta = (ClampMin = "0.0", Units = "cm"))
	float MaxOrbitRadius = 2400.0f;

	/** 플레이어의 새 방위각을 따라갈 최대 회전 속도입니다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RS|Camera", meta = (ClampMin = "0.0", Units = "deg/s"))
	float MaximumOrbitRotationSpeed = 180.0f;

	/** ViewTarget이 전환될 때 사용할 Blend 시간입니다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RS|Camera", meta = (ClampMin = "0.0", Units = "s"))
	float ViewTargetBlendTime = 0.6f;
};

/** 보스전 카메라가 구도를 계산하는 데 필요한 도메인 중립 입력입니다 */
struct FRSBossCameraContext
{
	/** 카메라가 공전할 중심이 되는 공간 기준 Component입니다 */
	TWeakObjectPtr<USceneComponent> PivotComponent;

	/** 방위각 계산의 기준이 되는 로컬 플레이어 Pawn입니다 */
	TWeakObjectPtr<APawn> TargetPlayer;

	/** 활성화한 보스전 공간에서 전달한 카메라 조정값입니다 */
	FRSBossCameraSettings Settings;
};
