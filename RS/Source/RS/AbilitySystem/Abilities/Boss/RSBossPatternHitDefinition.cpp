#include "RSBossPatternHitDefinition.h"

// 헤더가 UGameplayEffect를 전방 선언만 하므로 TSubclassOf의 형식 검사를 위해 완전한 타입이 필요합니다
#include "GameplayEffect.h"

bool FRSBossPatternHitSpec::IsValid() const
{
	return FMath::IsFinite(DamageAmount) && DamageAmount > 0.0f && DamageEffectClass;
}
