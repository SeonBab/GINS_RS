#pragma once

#include "CoreMinimal.h"
#include "Combat/RSCombatFunctionLibrary.h"
#include "RSBaseGameplayAbility.h"
#include "RSGameplayAbility_FireFieldDamage.generated.h"

class UGameplayEffect;

/** 화염 분화구 장판 이벤트 하나를 받아 플레이어 한 명에게 피해와 Reaction을 적용하고 즉시 끝납니다 */
UCLASS(Abstract, Blueprintable)
class RS_API URSGameplayAbility_FireFieldDamage : public URSBaseGameplayAbility
{
	GENERATED_BODY()

public:
	URSGameplayAbility_FireFieldDamage();

protected:
	/** Event Payload의 플레이어에게 한 번의 장판 피해와 Reaction을 적용합니다 */
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

#if WITH_EDITOR
	/** Damage GameplayEffect와 정수 피해량 설정을 검사합니다 */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	/** 한 번의 장판 Tick이 적용할 피해량입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|FlameCrater|Fire Field", meta = (AllowPrivateAccess = "true"))
	FScalableFloat Damage = 10.0f;

	/** 플레이어에게 적용할 공용 Damage GameplayEffect입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|FlameCrater|Fire Field", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	/** 장판 피해와 함께 요청할 피격 반응입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|FlameCrater|Fire Field", meta = (AllowPrivateAccess = "true"))
	FRSHitReactionDefinition Reaction;
};
