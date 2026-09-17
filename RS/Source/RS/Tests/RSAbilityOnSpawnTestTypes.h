// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#if WITH_TESTS

#include "CoreMinimal.h"
#include "Abilities/RSBaseGameplayAbility.h"
#include "RSAbilityOnSpawnTestTypes.generated.h"

/**
 * OnSpawn 정책을 가진 자동화 테스트 전용 어빌리티입니다
 * 실제 상시 어빌리티는 스캔 같은 부수 효과를 가지므로, 활성화 여부만 확인하도록 아무 일도 하지 않는 구체 클래스를 둡니다
 * UHT는 WITH_DEV_AUTOMATION_TESTS를 해석하지 못해 안쪽 선언을 건너뛰므로 WITH_TESTS로 감쌉니다
 */
UCLASS()
class URSAbilityOnSpawnTestAbility : public URSBaseGameplayAbility
{
	GENERATED_BODY()

public:
	URSAbilityOnSpawnTestAbility();
};

/** 자동 활성화 대상이 아님을 확인하기 위한 None 정책 어빌리티입니다 */
UCLASS()
class URSAbilityOnSpawnTestManualAbility : public URSBaseGameplayAbility
{
	GENERATED_BODY()

public:
	URSAbilityOnSpawnTestManualAbility();
};

#endif // WITH_TESTS
