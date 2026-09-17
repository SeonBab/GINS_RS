#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "RSAbilityTask_ScanInteractables.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRSInteractableFocusChangedSignature, AActor*, FocusedActor);

/**
 * Avatar 주변을 주기적으로 훑어 가장 가까운 상호작용 대상을 찾습니다
 * 선정 결과가 바뀐 경우에만 알리므로 구독자는 매 주기가 아니라 전환 시점만 처리합니다
 */
UCLASS()
class RS_API URSAbilityTask_ScanInteractables : public UAbilityTask
{
	GENERATED_BODY()

public:
	/** 주변 상호작용 대상을 주기적으로 탐색하는 AbilityTask를 생성합니다 */
	static URSAbilityTask_ScanInteractables* ScanInteractables(UGameplayAbility* OwningAbility, float ScanRadius, float ScanInterval, ECollisionChannel ScanChannel);

	virtual void Activate() override;
	virtual void OnDestroy(bool bInOwnerFinished) override;

public:
	/** 선정된 대상이 바뀌었음을 알리며 대상이 없으면 nullptr을 전달합니다 */
	UPROPERTY(BlueprintAssignable, Category = "RS|Interaction")
	FRSInteractableFocusChangedSignature OnFocusChanged;

private:
	/** 주변을 한 번 훑고 선정 결과가 바뀌었을 때만 알립니다 */
	void PerformScan();

private:
	float ScanRadius = 0.0f;
	float ScanInterval = 0.0f;
	ECollisionChannel ScanChannel = ECollisionChannel::ECC_MAX;

	/** 직전 주기에 선정한 대상이며 변화 감지에만 사용합니다 */
	TWeakObjectPtr<AActor> FocusedActor;

	FTimerHandle ScanTimerHandle;
};
