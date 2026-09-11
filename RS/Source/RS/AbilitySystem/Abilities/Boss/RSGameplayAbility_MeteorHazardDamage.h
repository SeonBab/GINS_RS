#pragma once

#include "CoreMinimal.h"
#include "RSBaseGameplayAbility.h"
#include "ScalableFloat.h"
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

#if WITH_EDITOR
	/** Damage GameplayEffect와 정수 피해량 설정을 검사합니다 */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	/** 한 번의 장판 Tick이 적용할 피해량입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Meteor Hazard|Damage", meta = (AllowPrivateAccess = "true"))
	FScalableFloat Damage = 10.0f;

	/** 플레이어에게 적용할 공용 Damage GameplayEffect입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Meteor Hazard|Damage", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGameplayEffect> DamageEffectClass;
};
