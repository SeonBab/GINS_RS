// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "RSGameModeBase.generated.h"

/** 프로젝트의 기본 플레이어 클래스와 인게임 HUD 구성을 지정합니다 */
UCLASS()
class RS_API ARSGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	/** 기본 PlayerState, Pawn, HUD 클래스를 설정합니다 */
	ARSGameModeBase();
};
