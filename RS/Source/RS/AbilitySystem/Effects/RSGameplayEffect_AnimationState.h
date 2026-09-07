// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "RSGameplayEffect_AnimationState.generated.h"

/** 애니메이션 구간이 지정한 상태 태그의 수명을 소유하는 공용 GameplayEffect입니다 */
UCLASS()
class RS_API URSGameplayEffect_AnimationStateLease : public UGameplayEffect
{
	GENERATED_BODY()

public:
	URSGameplayEffect_AnimationStateLease();
};
