// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RSMainMenuPlayerController.generated.h"

class URSMainMenuWidget;
class URSMainMenuSettingsWidget;
class URSMainMenuScoreboardWidget;
class URSQuitConfirmationWidget;
class URSScreenFadeWidget;
class UUserWidget;

/** Main Menu의 로컬 Widget, 입력과 화면 전환 수명을 관리합니다 */
UCLASS()
class RS_API ARSMainMenuPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	/** 화면을 어둡게 만들고 연출이 끝난 뒤 전달받은 동작을 실행합니다 */
	bool PlayScreenTransition(const FSimpleDelegate& OnFadedOut);

protected:
	/** Main Menu Widget을 생성하고 UI 입력을 준비합니다 */
	virtual void BeginPlay() override;

	/** 생성한 Widget을 정리합니다 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Main Menu Widget을 생성하고 Action 이벤트를 연결합니다 */
	bool CreateMainMenuWidget();

	/** 화면 전환 Widget을 생성하고 진입 Fade In을 시작합니다 */
	void CreateScreenFadeWidget();

	/** 지정한 Widget에 UI 입력과 Keyboard Focus를 전달합니다 */
	void ConfigureUserInterfaceInput(UUserWidget* FocusWidget);

	/** Main Menu를 다시 표시하고 게임 시작 Button에 Focus를 복구합니다 */
	void RestoreMainMenu();

	/** GameMode에 Stage 진입을 요청합니다 */
	void HandleStartGameRequested();

	/** 오디오 설정 화면을 생성하거나 다시 표시합니다 */
	void HandleAudioSettingsRequested();

	/** 스코어보드 화면을 생성하거나 다시 표시합니다 */
	void HandleScoreboardRequested();

	/** 종료 확인 화면을 생성하거나 다시 표시합니다 */
	void HandleQuitConfirmationRequested();

	/** Main Menu Settings 화면이 닫히면 Main Menu로 돌아갑니다 */
	void HandleMainMenuSettingsClosed();

	/** 스코어보드 화면이 닫히면 Main Menu로 돌아갑니다 */
	void HandleMainMenuScoreboardClosed();

	/** 종료 확인을 취소하고 Main Menu로 돌아갑니다 */
	void HandleQuitCancelled();

	/** 로컬 Application 종료를 요청합니다 */
	void HandleQuitConfirmed();

	/** 확정된 로컬 Application 종료를 실행합니다 */
	void QuitApplication();

private:
	/** Main Menu의 네 Action Button을 포함하는 Widget 클래스입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Main Menu", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<URSMainMenuWidget> MainMenuWidgetClass;

	/** 공용 오디오 설정 Panel과 Main Menu 닫기 Action을 포함하는 Widget 클래스입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Main Menu", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<URSMainMenuSettingsWidget> MainMenuSettingsWidgetClass;

	/** 공용 스코어보드 Panel과 Main Menu 닫기 Action을 포함하는 Widget 클래스입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Main Menu", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<URSMainMenuScoreboardWidget> MainMenuScoreboardWidgetClass;

	/** Application 종료 확인 Widget 클래스입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Main Menu", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<URSQuitConfirmationWidget> QuitConfirmationWidgetClass;

	/** 화면 전환 연출에 사용할 Widget 클래스입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Main Menu", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<URSScreenFadeWidget> ScreenFadeWidgetClass;

	/** 현재 Viewport에 사용하는 Main Menu Widget입니다 */
	UPROPERTY(Transient)
	TObjectPtr<URSMainMenuWidget> MainMenuWidget;

	/** 필요할 때 생성해 재사용하는 Main Menu Settings Widget입니다 */
	UPROPERTY(Transient)
	TObjectPtr<URSMainMenuSettingsWidget> MainMenuSettingsWidget;

	/** 필요할 때 생성해 재사용하는 Main Menu 스코어보드 Widget입니다 */
	UPROPERTY(Transient)
	TObjectPtr<URSMainMenuScoreboardWidget> MainMenuScoreboardWidget;

	/** 필요할 때 생성해 재사용하는 종료 확인 Widget입니다 */
	UPROPERTY(Transient)
	TObjectPtr<URSQuitConfirmationWidget> QuitConfirmationWidget;

	/** 현재 Viewport에 사용하는 화면 전환 Widget입니다 */
	UPROPERTY(Transient)
	TObjectPtr<URSScreenFadeWidget> ScreenFadeWidget;
};
