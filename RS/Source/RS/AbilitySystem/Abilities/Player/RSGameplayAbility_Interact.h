#pragma once

#include "CoreMinimal.h"
#include "RSBaseGameplayAbility.h"
#include "RSGameplayAbility_Interact.generated.h"

/**
 * 입력을 받아 지금 선택된 상호작용 대상을 실행합니다
 * 대상 탐색과 선택은 URSGameplayAbility_InteractionScan이 담당하므로 여기서는 조회만 합니다
 * 사망과 행동 잠금 판정은 ActivationBlockedTags로 GAS에 맡깁니다
 */
UCLASS()
class RS_API URSGameplayAbility_Interact : public URSBaseGameplayAbility
{
	GENERATED_BODY()

public:
	URSGameplayAbility_Interact();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

private:
	/**
	 * 대상을 실행하고 성공 여부를 반환합니다
	 * 대상이 어빌리티를 지정했으면 그것을 활성화하고, 아니면 대상의 Interact를 호출합니다
	 */
	bool TryInteractWithFocus(AActor* FocusedActor, AActor* InteractInstigator);
};
