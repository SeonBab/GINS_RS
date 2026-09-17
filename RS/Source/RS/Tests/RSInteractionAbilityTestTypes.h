// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#if WITH_TESTS

#include "CoreMinimal.h"
#include "RSGameplayAbility_InteractionScan.h"
#include "RSInteractionAbilityTestTypes.generated.h"

/** Blueprint 자식으로 부여되는 Scan Ability와 같은 클래스 상속 경로를 검증합니다 */
UCLASS()
class URSInteractionScanChildTestAbility : public URSGameplayAbility_InteractionScan
{
	GENERATED_BODY()
};

#endif // WITH_TESTS
