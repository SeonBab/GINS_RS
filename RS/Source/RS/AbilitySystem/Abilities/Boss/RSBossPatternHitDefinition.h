#pragma once

#include "CoreMinimal.h"
#include "Combat/RSCombatFunctionLibrary.h"
#include "ScalableFloat.h"
#include "RSBossPatternHitDefinition.generated.h"

class UGameplayEffect;

/** 보스 패턴 하나가 적중했을 때 적용할 피해와 피격 반응 설정입니다 */
USTRUCT(BlueprintType)
struct FRSBossPatternHitDefinition
{
	GENERATED_BODY()

	/** 대상에게 적용할 피해량입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Boss Pattern|Hit")
	FScalableFloat Damage = 10.0f;

	/** 대상에게 적용할 공용 대미지 GameplayEffect입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Boss Pattern|Hit")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	/** 피해와 함께 대상에게 요청할 피격 반응이며 None이면 피해만 적용합니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Boss Pattern|Hit")
	FRSHitReactionDefinition Reaction;
};

/** 생성된 장판 Actor가 Ability 종료 뒤에도 사용할 적중 설정의 실행별 스냅샷입니다 */
USTRUCT()
struct FRSBossPatternHitSpec
{
	GENERATED_BODY()

	/** 생성한 Ability 레벨에서 확정한 피해량입니다 */
	UPROPERTY(Transient)
	float DamageAmount = 0.0f;

	/** 대상에게 적용할 공용 대미지 GameplayEffect입니다 */
	UPROPERTY(Transient)
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	/** 피해와 독립적으로 대상에게 요청할 피격 반응입니다 */
	UPROPERTY(Transient)
	FRSHitReactionDefinition Reaction;

	/** 실행 중 안전하게 사용할 수 있는 스냅샷인지 반환합니다 */
	bool IsValid() const;
};
