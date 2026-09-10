#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "RSAbilityTask_WaitTelegraphFill.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRSTelegraphFillProgressSignature, float, Fill);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRSTelegraphFillFinishedSignature);

/** 지정한 시간 동안 0에서 1까지 Telegraph 진행률을 전달합니다 */
UCLASS()
class RS_API URSAbilityTask_WaitTelegraphFill : public UAbilityTask
{
	GENERATED_BODY()

public:
	URSAbilityTask_WaitTelegraphFill(const FObjectInitializer& ObjectInitializer);

	/** Telegraph Fill 진행률을 프레임마다 전달하는 AbilityTask를 생성합니다 */
	static URSAbilityTask_WaitTelegraphFill* WaitTelegraphFill(UGameplayAbility* OwningAbility, float Duration);

	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;

public:
	/** 현재 진행률이 바뀔 때 발생합니다 */
	UPROPERTY(BlueprintAssignable, Category = "RS|Telegraph")
	FRSTelegraphFillProgressSignature OnProgress;

	/** Fill 1을 전달한 직후 한 번 발생합니다 */
	UPROPERTY(BlueprintAssignable, Category = "RS|Telegraph")
	FRSTelegraphFillFinishedSignature OnFinished;

private:
	float Duration = 0.0f;
	float ElapsedTime = 0.0f;
	bool bHasFinished = false;
};
