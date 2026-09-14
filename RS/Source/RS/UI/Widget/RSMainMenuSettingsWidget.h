// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RSMainMenuSettingsWidget.generated.h"

class APlayerController;
class UButton;
class URSAudioSettingsWidget;

DECLARE_MULTICAST_DELEGATE(FRSMainMenuSettingsClosed)

/** 공용 오디오 설정 Panel과 Main Menu 복귀 Action을 조합합니다 */
UCLASS()
class RS_API URSMainMenuSettingsWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	/** Close Button 이벤트를 연결합니다 */
	virtual void NativeOnInitialized() override;

	/** Escape 입력으로 Main Menu 복귀를 요청합니다 */
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

public:
	/** 공용 오디오 설정 Panel을 현재 설정과 동기화합니다 */
	bool OpenSettings();

	/** 공용 오디오 설정 Panel의 Master Slider에 Keyboard Focus를 요청합니다 */
	void RequestInitialFocus(APlayerController* PlayerController);

	/** Main Menu 복귀 요청 이벤트를 반환합니다 */
	FRSMainMenuSettingsClosed& GetMainMenuSettingsClosed() { return OnMainMenuSettingsClosed; }

private:
	UFUNCTION()
	void HandleCloseButtonClicked();

private:
	/** Main Menu와 인게임에서 공유하는 오디오 설정 Panel입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "RS|Main Menu", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<URSAudioSettingsWidget> Widget_AudioSettings;

	/** 현재 설정을 유지하고 Main Menu로 돌아가는 Button입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "RS|Main Menu", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> Button_Close;

	/** Main Menu 복귀를 요청하는 이벤트입니다 */
	FRSMainMenuSettingsClosed OnMainMenuSettingsClosed;
};
