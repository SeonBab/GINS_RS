#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "RSAbilityTask_ObserveFacing.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FRSObserveFacingUpdatedSignature, bool, bHasFacingDirection, bool, bIsWithinYawTolerance, float, YawErrorDegrees);

/** 지정된 월드 위치를 향한 Avatar의 수평 Facing 상태를 매 프레임 전달합니다 */
UCLASS()
class RS_API URSAbilityTask_ObserveFacing : public UAbilityTask
{
	GENERATED_BODY()

public:
	URSAbilityTask_ObserveFacing(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 지정한 위치를 향한 수평 Facing을 관찰하는 AbilityTask를 생성합니다 */
	static URSAbilityTask_ObserveFacing* ObserveFacing(UGameplayAbility* OwningAbility, const FVector& FacingLocation, float YawToleranceDegrees);

	/** 다음 Tick부터 평가할 새로운 Facing 목표 위치를 저장합니다 */
	void SetFacingLocation(const FVector& NewFacingLocation);

	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;

public:
	/** 현재 Facing 방향의 유효성, 허용 오차 충족 여부와 signed 최단 Yaw 차이를 매 Tick 한 번 전달합니다 */
	UPROPERTY(BlueprintAssignable, Category = "RS|Facing")
	FRSObserveFacingUpdatedSignature OnFacingUpdated;

private:
	/** 지정된 Actor의 현재 수평 Facing 상태를 계산합니다 */
	static void CalculateFacingState(const AActor& AvatarActor, const FVector& TargetLocation, float ToleranceDegrees, bool& bOutHasFacingDirection, bool& bOutIsWithinYawTolerance, float& OutYawErrorDegrees);

private:
	/** 관찰할 Avatar이며 TargetActor를 직접 추적하지 않습니다 */
	TWeakObjectPtr<AActor> CachedAvatarActor;

	/** 현재 관찰하는 월드 공간 목표 위치입니다 */
	FVector FacingLocation = FVector::ZeroVector;

	/** Facing 완료로 간주할 절대 Yaw 오차입니다 */
	float YawToleranceDegrees = 0.0f;

	friend class FRSAbilityTaskObserveFacingTest;
};
