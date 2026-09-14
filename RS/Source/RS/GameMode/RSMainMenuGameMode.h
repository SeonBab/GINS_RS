// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "RSMainMenuGameMode.generated.h"

class ARSMainMenuPlayerController;
class USoundBase;

/** 전투용 Pawn과 HUD 없이 Main Menu의 Stage 진입을 관리합니다 */
UCLASS()
class RS_API ARSMainMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	/** Main Menu World를 전용 PlayerController와 플레이어 표현 없이 시작합니다 */
	ARSMainMenuGameMode();

protected:
	/** Main Menu 초기 음악을 요청한 뒤 플레이를 시작합니다 */
	virtual void StartPlay() override;

public:
	/** 유효한 로컬 Main Menu 요청을 한 번만 승인하고 Stage Level을 엽니다 */
	bool RequestStartGame(ARSMainMenuPlayerController* RequestingController);

	/** 게임 시작 대상으로 확정한 Level을 반환합니다 */
	const TSoftObjectPtr<UWorld>& GetStartLevel() const { return StartLevel; }

private:
	/** 현재 World의 초기 음악을 재생 관리자에 요청합니다 */
	void PlayInitialMusic();

private:
	/** 실제 게임에 사용할 Stage Level입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Main Menu", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<UWorld> StartLevel;

	/** Main Menu World가 시작될 때 재생할 음악입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Music", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<USoundBase> InitialMusic;

	/** 초기 음악이 재생될 때 적용할 Fade In 시간입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Music", meta = (ClampMin = "0.0", AllowPrivateAccess = "true"))
	float InitialMusicFadeDuration = 1.0f;

	/** 연속 입력으로 Level 전환을 여러 번 요청하지 않도록 최초 요청을 기록합니다 */
	bool bHasCommittedStartGame = false;
};
