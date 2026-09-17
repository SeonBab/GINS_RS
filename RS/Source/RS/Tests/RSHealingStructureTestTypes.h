// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#if WITH_TESTS

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "RSHealingStructureTestTypes.generated.h"

/**
 * 공용 회복 GameplayEffect와 같은 계약을 가지는 자동화 테스트 전용 GameplayEffect입니다
 * 실제 회복은 GE_Heal_Basic 에셋이므로 콘텐츠에 의존하지 않고 같은 SetByCaller 회복량 계약만 재현합니다
 * UHT는 WITH_DEV_AUTOMATION_TESTS를 해석하지 못해 안쪽 선언을 건너뛰므로 WITH_TESTS로 감쌉니다
 */
UCLASS()
class URSHealingStructureTestHealEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	URSHealingStructureTestHealEffect();
};

#endif // WITH_TESTS
