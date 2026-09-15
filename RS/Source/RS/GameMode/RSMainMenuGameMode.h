// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "RSMainMenuGameMode.generated.h"

class ARSMainMenuPlayerController;

/** 전투용 Pawn과 HUD 없이 Main Menu의 Stage 진입을 관리합니다 */
UCLASS()
class RS_API ARSMainMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	/** Main Menu World를 전용 PlayerController와 플레이어 표현 없이 시작합니다 */
	ARSMainMenuGameMode();

	/** 유효한 로컬 Main Menu 요청을 한 번만 승인하고 Stage Level을 엽니다 */
	bool RequestStartGame(ARSMainMenuPlayerController* RequestingController);

	/** 게임 시작 대상으로 확정한 Level을 반환합니다 */
	const TSoftObjectPtr<UWorld>& GetStartLevel() const { return StartLevel; }

private:
	/** 실제 게임에 사용할 Stage Level입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Main Menu", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<UWorld> StartLevel;

	/** 연속 입력으로 Level 전환을 여러 번 요청하지 않도록 최초 요청을 기록합니다 */
	bool bHasCommittedStartGame = false;
};
