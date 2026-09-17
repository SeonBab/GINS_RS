// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#if WITH_TESTS

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "RSHealthInitializationTestTypes.generated.h"

class URSHealthComponent;

/**
 * URSHealthComponent의 체력 변경 델리게이트를 수집하는 자동화 테스트 전용 객체입니다
 * 동적 델리게이트는 람다로 구독할 수 없어 UFUNCTION 핸들러를 가진 UObject가 필요합니다
 * UHT는 WITH_DEV_AUTOMATION_TESTS를 해석하지 못해 안쪽 선언을 건너뛰므로 WITH_TESTS로 감쌉니다
 */
UCLASS()
class URSHealthInitializationTestListener : public UObject
{
	GENERATED_BODY()

public:
	/** 전달된 HealthComponent의 현재 체력과 최대 체력 변경을 구독합니다 */
	void ListenTo(URSHealthComponent* InHealthComponent);

private:
	/** 현재 체력 변경 통지를 기록합니다 */
	UFUNCTION()
	void HandleHealthChanged(URSHealthComponent* InHealthComponent, float OldValue, float NewValue);

	/** 최대 체력 변경 통지를 기록합니다 */
	UFUNCTION()
	void HandleMaxHealthChanged(URSHealthComponent* InHealthComponent, float OldValue, float NewValue);

public:
	/** 수집한 현재 체력 통지의 새 값입니다 */
	TArray<float> HealthValues;

	/** 수집한 최대 체력 통지의 새 값입니다 */
	TArray<float> MaxHealthValues;
};

#endif // WITH_TESTS
