#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "RSAbilityTask_ObserveElapsedTime.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRSObserveElapsedTimeUpdatedSignature, float, ElapsedTime);

/** 지정한 Duration까지 실제 경과 시간을 매 프레임 전달합니다 */
UCLASS()
class RS_API URSAbilityTask_ObserveElapsedTime : public UAbilityTask
{
	GENERATED_BODY()

public:
	URSAbilityTask_ObserveElapsedTime(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 경과 시간을 관찰하는 AbilityTask를 생성합니다 */
	static URSAbilityTask_ObserveElapsedTime* ObserveElapsedTime(UGameplayAbility* OwningAbility, float Duration);

	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;

public:
	/** 0부터 Duration까지 누적한 실제 경과 시간을 전달합니다 */
	UPROPERTY(BlueprintAssignable, Category = "RS|Timing")
	FRSObserveElapsedTimeUpdatedSignature OnElapsedTimeUpdated;

private:
	float Duration = 0.0f;
	float ElapsedTime = 0.0f;
	bool bHasFinished = false;
};
