// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RSBaseGameplayAbility_GetUp.h"
#include "RSGameplayAbility_GetUpNormal.generated.h"

/**
 * 누운 시간이 다 되면 누움 어빌리티가 직접 실행하는 자동 기상입니다
 * 제자리에서 일어나므로 기반의 동작을 그대로 사용합니다
 */
UCLASS(Abstract, Blueprintable)
class RS_API URSGameplayAbility_GetUpNormal : public URSBaseGameplayAbility_GetUp
{
	GENERATED_BODY()

public:
	URSGameplayAbility_GetUpNormal();
};
