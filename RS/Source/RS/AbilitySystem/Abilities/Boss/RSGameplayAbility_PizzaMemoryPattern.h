#pragma once

#include "CoreMinimal.h"
#include "RSBaseGameplayAbility_BossPattern.h"
#include "RSPizzaMemoryPatternDefinition.h"
#include "RSGameplayAbility_PizzaMemoryPattern.generated.h"

/** 위험 조각의 순서를 기억한 뒤 같은 순서로 회피하는 피자 패턴 Ability입니다 */
UCLASS(Abstract, Blueprintable)
class RS_API URSGameplayAbility_PizzaMemoryPattern : public URSBaseGameplayAbility_BossPattern
{
	GENERATED_BODY()

public:
	URSGameplayAbility_PizzaMemoryPattern();

protected:
	/** 실행 흐름이 구현되기 전 생성한 에셋이 활성 상태로 남지 않도록 즉시 취소합니다 */
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

#if WITH_EDITOR
	/** 안전지대 후보 목록이 런타임에서 선택 가능한 형태인지 검사합니다 */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

protected:
	/** 에디터에서 설정할 안전지대 순서 후보입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Pizza Memory Pattern")
	FRSPizzaMemoryPatternDefinition PizzaMemoryPatternDefinition;
};
