// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RSPlayerController.generated.h"

class URSLocalPlayerViewModelSubsystem;

/** 마우스 위치를 게임플레이 입력에 제공하고 로컬 플레이어의 ViewModel을 게임플레이 데이터와 연결합니다 */
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

	/**
	 * 마우스 커서 아래의 유효한 월드 위치를 반환합니다
	 * 위치 조회만 담당하며 Character의 이동 또는 어빌리티 실행 상태를 변경하지 않습니다
	 */
	bool GetCursorWorldLocation(FVector& OutCursorWorldLocation) const;

private:
	/** 로컬 플레이어가 월드와 UI를 마우스로 조작할 수 있도록 커서와 입력 모드를 설정합니다 */
	void ConfigureMouseInput();

	/** 현재 로컬 플레이어의 ViewModel 저장소를 반환합니다 */
	URSLocalPlayerViewModelSubsystem* GetViewModelSubsystem() const;

	/** 현재 PlayerCharacter의 HealthComponent를 PlayerStatusViewModel에 연결합니다 */
	void UpdatePlayerStatusViewModelSource();
};
