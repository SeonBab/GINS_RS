// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RSPlayerController.generated.h"

class URSLocalPlayerViewModelSubsystem;

/** 플레이어 입력을 처리하고 로컬 플레이어의 ViewModel을 게임플레이 데이터와 연결합니다 */
UCLASS()
class RS_API ARSPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	/** 로컬 플레이어의 공유 ViewModel을 준비하고 현재 Pawn의 데이터 원본을 연결합니다 */
	virtual void BeginPlay() override;

	/** PlayerController가 연결한 ViewModel의 데이터 원본과 이벤트 구독을 해제합니다 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	/** Pawn이 변경되면 PlayerStatusViewModel의 데이터 원본을 갱신합니다 */
	virtual void SetPawn(APawn* InPawn) override;

	/** 매 프레임 기록된 어빌리티 입력을 처리합니다 */
	virtual void PostProcessInput(float DeltaTime, bool bGamePaused) override;

private:
	/** 현재 로컬 플레이어의 ViewModel 저장소를 반환합니다 */
	URSLocalPlayerViewModelSubsystem* GetViewModelSubsystem() const;

	/** 현재 PlayerCharacter의 HealthComponent를 PlayerStatusViewModel에 연결합니다 */
	void UpdatePlayerStatusViewModelSource();
};
