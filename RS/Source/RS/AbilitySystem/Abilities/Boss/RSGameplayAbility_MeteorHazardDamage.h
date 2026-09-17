#pragma once

#include "CoreMinimal.h"
#include "RSBaseGameplayAbility.h"
#include "RSGameplayAbility_MeteorHazardDamage.generated.h"

class UGameplayEffect;

/** 운석 장판 이벤트 하나를 받아 플레이어 한 명에게 피해만 적용하고 즉시 끝납니다 */
UCLASS(Abstract, Blueprintable)
class RS_API URSGameplayAbility_MeteorHazardDamage : public URSBaseGameplayAbility
{
	GENERATED_BODY()

public:
	URSGameplayAbility_MeteorHazardDamage();

protected:
	/** Event Payload의 플레이어에게 한 번의 장판 피해를 적용합니다 */
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

};
