// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RSInGameMenuWidget.generated.h"

class APlayerController;
class UButton;
class UWidget;
class UWidgetSwitcher;
class URSAudioSettingsWidget;
class URSScoreboardPanelWidget;

DECLARE_MULTICAST_DELEGATE(FRSInGameMenuActionRequested)

/** 공용 오디오 설정 Panel과 인게임 전용 Action을 조합합니다 */
UCLASS()
class RS_API URSInGameMenuWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	/** 인게임 Action Button의 클릭 이벤트를 연결합니다 */
	virtual void NativeOnInitialized() override;

	/** 하위 페이지의 Escape는 기본 페이지 복귀로, 기본 페이지의 Escape는 게임 계속 요청으로 처리합니다 */
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	/** 열려 있는 스코어보드 구독을 해제합니다 */
	virtual void NativeDestruct() override;

public:
	/** 기본 Action 페이지로 메뉴를 엽니다 */
	bool OpenMenu();

	/** 게임 계속 Button에 Keyboard Focus를 요청합니다 */
	void RequestInitialFocus(APlayerController* PlayerController);

	/** 다섯 인게임 Action Button의 입력 가능 상태를 함께 변경합니다 */
	void SetActionsEnabled(bool bEnabled);

	/** 게임 계속 요청 이벤트를 반환합니다 */
	FRSInGameMenuActionRequested& GetContinueRequested() { return OnContinueRequested; }

	/** 현재 Level 재시작 요청 이벤트를 반환합니다 */
	FRSInGameMenuActionRequested& GetRestartRequested() { return OnRestartRequested; }

	/** Main Menu 이동 요청 이벤트를 반환합니다 */
	FRSInGameMenuActionRequested& GetMainMenuRequested() { return OnMainMenuRequested; }

private:
	UFUNCTION()
	void HandleContinueButtonClicked();

	UFUNCTION()
	void HandleScoreboardButtonClicked();

	UFUNCTION()
	void HandleSettingsButtonClicked();

	UFUNCTION()
	void HandleBackButtonClicked();

	UFUNCTION()
	void HandleRestartButtonClicked();

	UFUNCTION()
	void HandleMainMenuButtonClicked();

private:
	/** 기본 Action·설정·스코어보드 페이지를 전환합니다 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "RS|In Game Menu", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UWidgetSwitcher> WidgetSwitcher_MenuPages;

	/** 게임 계속 등 기본 Action을 표시하는 페이지입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "RS|In Game Menu", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> Panel_ActionPage;

	/** 공용 오디오 설정 Panel을 포함하는 하위 페이지입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "RS|In Game Menu", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> Panel_SettingsPage;

	/** 공용 스코어보드 Panel을 포함하는 하위 페이지입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "RS|In Game Menu", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> Panel_ScoreboardPage;

	/** Main Menu와 인게임에서 공유하는 오디오 설정 Panel입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "RS|In Game Menu", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<URSAudioSettingsWidget> Widget_AudioSettings;

	/** Main Menu와 인게임에서 공유하는 스코어보드 Panel입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "RS|In Game Menu", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<URSScoreboardPanelWidget> Widget_Scoreboard;

	/** Pause를 해제하고 게임플레이로 돌아가는 Button입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "RS|In Game Menu", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> Button_Continue;

	/** 누적 클리어 기록 페이지를 여는 Button입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "RS|In Game Menu", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> Button_Scoreboard;

	/** 오디오 설정 페이지를 여는 Button입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "RS|In Game Menu", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> Button_Settings;

	/** 현재 Level을 다시 시작하는 Button입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "RS|In Game Menu", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> Button_Restart;

	/** Main Menu Level로 이동하는 Button입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "RS|In Game Menu", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> Button_MainMenu;

	/** 설정 페이지에서 기본 Action 페이지로 돌아가는 Button입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "RS|In Game Menu", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> Button_BackFromSettings;

	/** 스코어보드 페이지에서 기본 Action 페이지로 돌아가는 Button입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "RS|In Game Menu", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> Button_BackFromScoreboard;

	/** 게임 계속 의도를 HUD에 전달하는 이벤트입니다 */
	FRSInGameMenuActionRequested OnContinueRequested;

	/** 현재 Level 재시작 의도를 HUD에 전달하는 이벤트입니다 */
	FRSInGameMenuActionRequested OnRestartRequested;

	/** Main Menu 이동 의도를 HUD에 전달하는 이벤트입니다 */
	FRSInGameMenuActionRequested OnMainMenuRequested;
};
