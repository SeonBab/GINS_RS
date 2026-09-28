// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RSBaseGameplayAbility.h"
#include "RSBaseGameplayAbility_Attack.generated.h"

class UGameplayEffect;

/** 대상에게 GameplayEffect 기반 피해를 적용하는 공격 어빌리티의 공통 기반입니다 */
UCLASS(Abstract, Blueprintable)
class RS_API URSBaseGameplayAbility_Attack : public URSBaseGameplayAbility
{
	GENERATED_BODY()

protected:
	/** 판정에 걸린 대상 하나에게 실행별 피해량을 담은 GameplayEffect를 적용합니다 */
	virtual void ApplyDamageToTarget(AActor* TargetActor, TSubclassOf<UGameplayEffect> DamageEffectClass, float DamageAmount);
};
