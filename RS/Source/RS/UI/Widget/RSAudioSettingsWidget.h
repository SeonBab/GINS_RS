// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RSAudioVolumeSettings.h"
#include "RSAudioSettingsWidget.generated.h"

class APlayerController;
class UButton;
class USlider;
class UTextBlock;
class URSAudioSettingsSubsystem;

DECLARE_MULTICAST_DELEGATE(FRSAudioSettingsClosed)

/** Master·배경음·효과음의 임시 적용과 확정·취소 입력을 관리합니다 */
UCLASS()
class RS_API URSAudioSettingsWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	/** Slider와 Action Button의 이벤트를 연결합니다 */
	virtual void NativeOnInitialized() override;

	/** Widget이 예상하지 않게 제거되면 임시 음량을 저장값으로 복구합니다 */
	virtual void NativeDestruct() override;

	/** Escape 입력을 취소로 처리합니다 */
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

public:
	/** 저장값 Snapshot을 보관하고 설정 화면 편집을 시작합니다 */
	bool OpenAudioSettings();

	/** Master Slider에 Keyboard Focus를 요청합니다 */
	void RequestInitialFocus(APlayerController* PlayerController);

	/** 설정 화면이 적용 또는 취소로 닫혔음을 알리는 이벤트를 반환합니다 */
	FRSAudioSettingsClosed& GetAudioSettingsClosed() { return OnAudioSettingsClosed; }

private:
	/** 현재 세 Slider 값을 Runtime 출력에 임시 적용합니다 */
	void PreviewCurrentSliderValues();

	/** 지정한 설정을 Slider에 반영합니다 */
	void SynchronizeSliders(const FRSAudioVolumeSettings& Settings);

	/** 현재 Slider 값을 퍼센트 Text에 반영합니다 */
	void UpdateValueTexts();

	/** 현재 GameInstance의 오디오 설정 Subsystem을 반환합니다 */
	URSAudioSettingsSubsystem* GetAudioSettingsSubsystem() const;

	UFUNCTION()
	void HandleSliderValueChanged(float Value);

	UFUNCTION()
	void HandleApplyButtonClicked();

	UFUNCTION()
	void HandleCancelButtonClicked();

	UFUNCTION()
	void HandleDefaultsButtonClicked();

private:
	/** 모든 오디오 채널에 공통 배율을 지정하는 Slider입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "RS|Audio", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<USlider> Slider_Master;

	/** 배경음 채널의 상대 배율을 지정하는 Slider입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "RS|Audio", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<USlider> Slider_BackgroundMusic;

	/** 효과음과 UI 오디오 채널의 상대 배율을 지정하는 Slider입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "RS|Audio", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<USlider> Slider_SoundEffects;

	/** Master Slider 값을 퍼센트로 표시하는 Text입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "RS|Audio", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> Text_MasterValue;

	/** 배경음 Slider 값을 퍼센트로 표시하는 Text입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "RS|Audio", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> Text_BackgroundMusicValue;

	/** 효과음 Slider 값을 퍼센트로 표시하는 Text입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "RS|Audio", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> Text_SoundEffectsValue;

	/** 임시 적용값을 저장하고 설정 화면을 닫는 Button입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "RS|Audio", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> Button_Apply;

	/** 저장값 Snapshot으로 복구하고 설정 화면을 닫는 Button입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "RS|Audio", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> Button_Cancel;

	/** 세 편집값과 Runtime 출력을 기본값으로 임시 변경하는 Button입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "RS|Audio", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> Button_Defaults;

	/** 적용 또는 취소 후 Main Menu 복귀를 요청하는 이벤트입니다 */
	FRSAudioSettingsClosed OnAudioSettingsClosed;

	/** 코드가 Slider를 동기화할 때 Preview 이벤트가 발생하지 않도록 구분합니다 */
	bool bIsSynchronizingSliders = false;
};
