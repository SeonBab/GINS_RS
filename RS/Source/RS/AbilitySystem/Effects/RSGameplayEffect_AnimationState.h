// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "RSGameplayEffect_AnimationState.generated.h"

/** 실행 주체가 명시적으로 회수할 상태 태그의 수명을 소유하는 공용 GameplayEffect입니다 */
UCLASS()
class RS_API URSGameplayEffect_StateLease : public UGameplayEffect
{
	GENERATED_BODY()

public:
	URSGameplayEffect_StateLease();
};

/** 애니메이션 Notify State가 지정한 상태 태그를 소유하는 GameplayEffect입니다 */
UCLASS()
class RS_API URSGameplayEffect_AnimationStateLease : public URSGameplayEffect_StateLease
{
	GENERATED_BODY()
};
