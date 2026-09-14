// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RSMainMenuWidget.generated.h"

class APlayerController;
class UButton;

DECLARE_MULTICAST_DELEGATE(FRSMainMenuActionRequested)

/** Main Menu의 세 Action 의도를 PlayerController에 전달합니다 */
UCLASS()
class RS_API URSMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	/** 저장 에셋에 저작한 Action Button의 클릭 이벤트를 연결합니다 */
	virtual void NativeOnInitialized() override;

public:
	/** 게임 시작 요청 이벤트를 반환합니다 */
	FRSMainMenuActionRequested& GetStartGameRequested() { return OnStartGameRequested; }

	/** 설정 화면 요청 이벤트를 반환합니다 */
	FRSMainMenuActionRequested& GetAudioSettingsRequested() { return OnAudioSettingsRequested; }

	/** 종료 확인 요청 이벤트를 반환합니다 */
	FRSMainMenuActionRequested& GetQuitConfirmationRequested() { return OnQuitConfirmationRequested; }

	/** 세 Action Button의 입력 가능 상태를 함께 변경합니다 */
	void SetActionsEnabled(bool bEnabled);

	/** 게임 시작 Button에 Keyboard Focus를 요청합니다 */
	void RequestInitialFocus(APlayerController* PlayerController);

private:
	UFUNCTION()
	void HandleStartGameButtonClicked();

	UFUNCTION()
	void HandleAudioSettingsButtonClicked();

	UFUNCTION()
	void HandleQuitGameButtonClicked();

private:
	/** 실제 게임 Level 진입을 요청하는 Button입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "RS|Main Menu", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> Button_StartGame;

	/** 오디오 설정 화면 표시를 요청하는 Button입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "RS|Main Menu", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> Button_AudioSettings;

	/** Application 종료 확인을 요청하는 Button입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "RS|Main Menu", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> Button_QuitGame;

	/** 게임 시작 의도를 PlayerController에 전달합니다 */
	FRSMainMenuActionRequested OnStartGameRequested;

	/** 설정 화면 표시 의도를 PlayerController에 전달합니다 */
	FRSMainMenuActionRequested OnAudioSettingsRequested;

	/** 종료 확인 화면 표시 의도를 PlayerController에 전달합니다 */
	FRSMainMenuActionRequested OnQuitConfirmationRequested;
};
