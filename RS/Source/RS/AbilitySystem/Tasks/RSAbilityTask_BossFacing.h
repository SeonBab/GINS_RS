#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "RSAbilityTask_BossFacing.generated.h"

class ARSBossCharacter;
class ARSBossController;

/** 보스가 목표 방향을 정하는 방식을 나타냅니다 */
enum class ERSBossFacingMode : uint8
{
	TrackActor,
	FixedWorldYaw
};

/** 보스 Facing이 끝난 이유를 나타냅니다 */
UENUM(BlueprintType)
enum class ERSBossFacingResult : uint8
{
	Aligned,
	TimedOut,
	NoDirection,
	TargetInvalid,
	InvalidContext
};

/** 한 번의 보스 Facing 실행에 필요한 런타임 요청입니다 */
struct FRSBossFacingRequest
{
	/** 지정한 Actor의 위치 Snapshot을 향하고 이동량이 크면 새 Snapshot으로 갱신하는 요청을 만듭니다 */
	static FRSBossFacingRequest MakeTrackActor(AActor* TargetActor, float RotationSpeedDegreesPerSecond, float YawToleranceDegrees, float TargetDriftTolerance, float MaxDuration);

	/** 지정한 월드 절대 Yaw를 향하고 완료 시 정확한 값으로 보정하는 요청을 만듭니다 */
	static FRSBossFacingRequest MakeFixedWorldYaw(float WorldYawDegrees, float RotationSpeedDegreesPerSecond, float YawToleranceDegrees, float MaxDuration);

	/** 실행에 필요한 값이 유효한지 반환합니다 */
	bool IsDataValid() const;

	ERSBossFacingMode Mode = ERSBossFacingMode::TrackActor;
	TWeakObjectPtr<AActor> TargetActor;
	float WorldYawDegrees = 0.0f;
	float RotationSpeedDegreesPerSecond = 150.0f;
	float YawToleranceDegrees = 3.0f;
	float TargetDriftTolerance = 100.0f;
	float MaxDuration = 1.5f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRSBossFacingFinishedSignature, ERSBossFacingResult, Result, float, FinalYawDegrees);

/** 보스의 Actor 추적 또는 월드 절대 Yaw Facing과 회전 설정 수명을 관리합니다 */
UCLASS()
class RS_API URSAbilityTask_BossFacing : public UAbilityTask
{
	GENERATED_BODY()

public:
	URSAbilityTask_BossFacing(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 전달한 요청을 실행할 AbilityTask를 생성합니다 */
	static URSAbilityTask_BossFacing* CreateBossFacingTask(UGameplayAbility* OwningAbility, const FRSBossFacingRequest& Request);

	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;
	virtual void OnDestroy(bool bInOwnerFinished) override;

public:
	/** Facing이 완료되거나 더 진행할 수 없을 때 결과와 마지막 Actor Yaw를 전달합니다 */
	UPROPERTY(BlueprintAssignable, Category = "RS|Facing")
	FRSBossFacingFinishedSignature OnFacingFinished;

private:
	/** Actor 추적 요청의 현재 Snapshot을 평가하고 필요하면 갱신합니다 */
	void TickActorTracking();

	/** 월드 절대 Yaw 요청을 평가하고 완료 또는 timeout에서 정확한 Yaw로 보정합니다 */
	void TickFixedWorldYaw();

	/** FixedWorldYaw가 이동 중에도 같은 방향을 유지하도록 현재 위치에서 FocalPoint를 갱신합니다 */
	void UpdateFixedWorldFocalPoint();

	/** Actor와 Controller의 Yaw를 목표 월드 값으로 정확히 맞춥니다 */
	void ApplyExactWorldYaw();

	/** 성공한 Facing이 공격 동안 유지되도록 Focus를 해제하고 Controller 기반 회전을 잠급니다 */
	void LockCompletedFacing();

	/** Task가 소유한 Focus와 회전 설정을 원래 상태로 되돌립니다 */
	void RestoreFacingState();

	/** 결과를 한 번 전달하고 성공한 방향 또는 실패한 원래 상태를 유지합니다 */
	void FinishFacing(ERSBossFacingResult Result, bool bShouldLockFacing);

	/** 지정한 두 Yaw의 signed 최단 오차와 허용 오차 충족 여부를 계산합니다 */
	static void CalculateYawState(float CurrentYawDegrees, float TargetYawDegrees, float ToleranceDegrees, bool& bOutIsWithinYawTolerance, float& OutYawErrorDegrees);

	/** 현재 회전에서 Yaw만 정규화한 목표 값으로 교체합니다 */
	static FRotator MakeExactYawRotation(const FRotator& CurrentRotation, float TargetYawDegrees);

private:
	FRSBossFacingRequest FacingRequest;
	TWeakObjectPtr<ARSBossCharacter> BossCharacter;
	TWeakObjectPtr<ARSBossController> BossController;
	FVector TargetSnapshotLocation = FVector::ZeroVector;
	FRotator OriginalRotationRate = FRotator::ZeroRotator;
	bool bOriginalUseControllerDesiredRotation = false;
	bool bHasSavedRotationState = false;
	bool bHasAppliedGameplayFocus = false;
	bool bHasFinished = false;
	double StartTimeSeconds = 0.0;

	friend class FRSAbilityTaskBossFacingTest;
};
