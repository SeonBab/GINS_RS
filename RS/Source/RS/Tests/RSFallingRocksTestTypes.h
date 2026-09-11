// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/RSGameplayAbility_RandomFallingRocks.h"
#include "RSFallingRocksTestTypes.generated.h"

/** 페이즈 진입과 반복 예약 자동화에서 사용할 구체 낙석 Ability입니다 */
UCLASS()
class URSFallingRocksTestAbility : public URSGameplayAbility_RandomFallingRocks
{
	GENERATED_BODY()

public:
	URSFallingRocksTestAbility();

	/** 이 인스턴스가 활성화된 횟수를 반환합니다 */
	int32 GetActivationCount() const { return ActivationCount; }

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

private:
	int32 ActivationCount = 0;
};
