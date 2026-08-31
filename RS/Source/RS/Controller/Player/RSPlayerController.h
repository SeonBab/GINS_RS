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
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	virtual void SetPawn(APawn* InPawn) override;

	/** 매 프레임 기록된 어빌리티 입력을 처리합니다 */
	virtual void PostProcessInput(float DeltaTime, bool bGamePaused) override;

private:
	URSLocalPlayerViewModelSubsystem* GetViewModelSubsystem() const;

	void UpdatePlayerStatusViewModelSource();
};
