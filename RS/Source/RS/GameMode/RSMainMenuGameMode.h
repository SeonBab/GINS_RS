// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "RSMainMenuGameMode.generated.h"

/** 전투용 Pawn과 HUD를 생성하지 않는 Main Menu Map의 최소 GameMode입니다 */
UCLASS()
class RS_API ARSMainMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	/** Main Menu World를 플레이어 표현 없이 시작하도록 기본 클래스를 비웁니다 */
	ARSMainMenuGameMode();
};
