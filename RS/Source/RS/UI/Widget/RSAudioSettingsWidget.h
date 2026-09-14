// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RSAudioVolumeSettings.h"
#include "RSAudioSettingsWidget.generated.h"

class APlayerController;
class USlider;
class UTextBlock;
class URSAudioSettingsSubsystem;

/** Master·배경음·효과음의 즉시 적용과 조작 종료 저장을 관리합니다 */
UCLASS()
class RS_API URSAudioSettingsWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	/** Slider의 변경과 Mouse Capture 이벤트를 연결합니다 */
	virtual void NativeOnInitialized() override;

	/** Widget이 제거될 때 남은 변경값을 저장합니다 */
	virtual void NativeDestruct() override;

public:
	/** 현재 적용 중인 값을 Slider에 동기화합니다 */
	bool OpenAudioSettings();

	/** Master Slider에 Keyboard Focus를 요청합니다 */
	void RequestInitialFocus(APlayerController* PlayerController);

private:
	/** 현재 세 Slider 값을 Runtime 출력에 즉시 적용합니다 */
	void ApplyCurrentSliderValues();

	/** 변경된 현재 값을 사용자 설정에 저장합니다 */
	void SaveAudioSettingsIfNeeded();

	/** 지정한 설정을 Slider에 반영합니다 */
	void SynchronizeSliders(const FRSAudioVolumeSettings& Settings);

	/** 현재 Slider 값을 퍼센트 Text에 반영합니다 */
	void UpdateValueTexts();

	/** 현재 GameInstance의 오디오 설정 Subsystem을 반환합니다 */
	URSAudioSettingsSubsystem* GetAudioSettingsSubsystem() const;

	UFUNCTION()
	void HandleSliderValueChanged(float Value);

	UFUNCTION()
	void HandleSliderMouseCaptureBegin();

	UFUNCTION()
	void HandleSliderMouseCaptureEnd();

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

	/** 코드가 Slider를 동기화할 때 적용 이벤트가 발생하지 않도록 구분합니다 */
	bool bIsSynchronizingSliders = false;

	/** 마우스 드래그 중 반복 저장을 피하기 위해 Capture 상태를 구분합니다 */
	bool bIsSliderMouseCaptured = false;

	/** Runtime에 반영됐지만 사용자 설정 파일에는 아직 저장하지 않은 변경이 있는지 나타냅니다 */
	bool bHasUnsavedChanges = false;
};
