// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RSMainMenuPlayerController.generated.h"

class URSMainMenuWidget;
class URSMainMenuSettingsWidget;
class URSQuitConfirmationWidget;
class UUserWidget;

/** Main Menu의 로컬 Widget, 입력과 화면 전환 수명을 관리합니다 */
UCLASS()
class RS_API ARSMainMenuPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	/** Main Menu Widget을 생성하고 UI 입력을 준비합니다 */
	virtual void BeginPlay() override;

	/** 생성한 Widget을 정리합니다 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Main Menu Widget을 생성하고 Action 이벤트를 연결합니다 */
	bool CreateMainMenuWidget();

	/** 지정한 Widget에 UI 입력과 Keyboard Focus를 전달합니다 */
	void ConfigureUserInterfaceInput(UUserWidget* FocusWidget);

	/** Main Menu를 다시 표시하고 게임 시작 Button에 Focus를 복구합니다 */
	void RestoreMainMenu();

	/** GameMode에 Stage 진입을 요청합니다 */
	void HandleStartGameRequested();

	/** 오디오 설정 화면을 생성하거나 다시 표시합니다 */
	void HandleAudioSettingsRequested();

	/** 종료 확인 화면을 생성하거나 다시 표시합니다 */
	void HandleQuitConfirmationRequested();

	/** Main Menu Settings 화면이 닫히면 Main Menu로 돌아갑니다 */
	void HandleMainMenuSettingsClosed();

	/** 종료 확인을 취소하고 Main Menu로 돌아갑니다 */
	void HandleQuitCancelled();

	/** 로컬 Application 종료를 요청합니다 */
	void HandleQuitConfirmed();

private:
	/** Main Menu의 세 Action Button을 포함하는 Widget 클래스입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Main Menu", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<URSMainMenuWidget> MainMenuWidgetClass;

	/** 공용 오디오 설정 Panel과 Main Menu 닫기 Action을 포함하는 Widget 클래스입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Main Menu", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<URSMainMenuSettingsWidget> MainMenuSettingsWidgetClass;

	/** Application 종료 확인 Widget 클래스입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Main Menu", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<URSQuitConfirmationWidget> QuitConfirmationWidgetClass;

	/** 현재 Viewport에 사용하는 Main Menu Widget입니다 */
	UPROPERTY(Transient)
	TObjectPtr<URSMainMenuWidget> MainMenuWidget;

	/** 필요할 때 생성해 재사용하는 Main Menu Settings Widget입니다 */
	UPROPERTY(Transient)
	TObjectPtr<URSMainMenuSettingsWidget> MainMenuSettingsWidget;

	/** 필요할 때 생성해 재사용하는 종료 확인 Widget입니다 */
	UPROPERTY(Transient)
	TObjectPtr<URSQuitConfirmationWidget> QuitConfirmationWidget;
};
