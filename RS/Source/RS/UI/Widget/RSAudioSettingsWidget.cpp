// Fill out your copyright notice in the Description page of Project Settings.

#include "RSAudioSettingsWidget.h"

#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "RSAudioSettingsSubsystem.h"

void URSAudioSettingsWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (Slider_Master)
	{
		Slider_Master->OnValueChanged.AddUniqueDynamic(this, &ThisClass::HandleSliderValueChanged);
		Slider_Master->OnMouseCaptureBegin.AddUniqueDynamic(this, &ThisClass::HandleSliderMouseCaptureBegin);
		Slider_Master->OnMouseCaptureEnd.AddUniqueDynamic(this, &ThisClass::HandleSliderMouseCaptureEnd);
	}

	if (Slider_BackgroundMusic)
	{
		Slider_BackgroundMusic->OnValueChanged.AddUniqueDynamic(this, &ThisClass::HandleSliderValueChanged);
		Slider_BackgroundMusic->OnMouseCaptureBegin.AddUniqueDynamic(this, &ThisClass::HandleSliderMouseCaptureBegin);
		Slider_BackgroundMusic->OnMouseCaptureEnd.AddUniqueDynamic(this, &ThisClass::HandleSliderMouseCaptureEnd);
	}

	if (Slider_SoundEffects)
	{
		Slider_SoundEffects->OnValueChanged.AddUniqueDynamic(this, &ThisClass::HandleSliderValueChanged);
		Slider_SoundEffects->OnMouseCaptureBegin.AddUniqueDynamic(this, &ThisClass::HandleSliderMouseCaptureBegin);
		Slider_SoundEffects->OnMouseCaptureEnd.AddUniqueDynamic(this, &ThisClass::HandleSliderMouseCaptureEnd);
	}

}

void URSAudioSettingsWidget::NativeDestruct()
{
	SaveAudioSettingsIfNeeded();

	Super::NativeDestruct();
}

bool URSAudioSettingsWidget::OpenAudioSettings()
{
	URSAudioSettingsSubsystem* AudioSettingsSubsystem = GetAudioSettingsSubsystem();
	if (!AudioSettingsSubsystem)
	{
		return false;
	}

	SynchronizeSliders(AudioSettingsSubsystem->GetCurrentAudioSettings());
	bIsSliderMouseCaptured = false;
	bHasUnsavedChanges = false;

	return true;
}

void URSAudioSettingsWidget::RequestInitialFocus(APlayerController* PlayerController)
{
	if (Slider_Master && PlayerController)
	{
		Slider_Master->SetUserFocus(PlayerController);
	}
}

void URSAudioSettingsWidget::ApplyCurrentSliderValues()
{
	if (!Slider_Master || !Slider_BackgroundMusic || !Slider_SoundEffects)
	{
		return;
	}

	FRSAudioVolumeSettings Settings;
	Settings.MasterVolume = Slider_Master->GetValue();
	Settings.BackgroundMusicVolume = Slider_BackgroundMusic->GetValue();
	Settings.SoundEffectsVolume = Slider_SoundEffects->GetValue();

	if (URSAudioSettingsSubsystem* AudioSettingsSubsystem = GetAudioSettingsSubsystem())
	{
		AudioSettingsSubsystem->SetAudioSettings(Settings);
		bHasUnsavedChanges = true;
	}
}

void URSAudioSettingsWidget::SaveAudioSettingsIfNeeded()
{
	if (!bHasUnsavedChanges)
	{
		return;
	}

	if (URSAudioSettingsSubsystem* AudioSettingsSubsystem = GetAudioSettingsSubsystem())
	{
		if (AudioSettingsSubsystem->SaveAudioSettings())
		{
			bHasUnsavedChanges = false;
		}
	}
}

void URSAudioSettingsWidget::SynchronizeSliders(const FRSAudioVolumeSettings& Settings)
{
	bIsSynchronizingSliders = true;

	if (Slider_Master)
	{
		Slider_Master->SetValue(Settings.MasterVolume);
	}

	if (Slider_BackgroundMusic)
	{
		Slider_BackgroundMusic->SetValue(Settings.BackgroundMusicVolume);
	}

	if (Slider_SoundEffects)
	{
		Slider_SoundEffects->SetValue(Settings.SoundEffectsVolume);
	}

	bIsSynchronizingSliders = false;
	UpdateValueTexts();
}

void URSAudioSettingsWidget::UpdateValueTexts()
{
	if (Text_MasterValue && Slider_Master)
	{
		Text_MasterValue->SetText(FText::AsPercent(Slider_Master->GetValue()));
	}

	if (Text_BackgroundMusicValue && Slider_BackgroundMusic)
	{
		Text_BackgroundMusicValue->SetText(FText::AsPercent(Slider_BackgroundMusic->GetValue()));
	}

	if (Text_SoundEffectsValue && Slider_SoundEffects)
	{
		Text_SoundEffectsValue->SetText(FText::AsPercent(Slider_SoundEffects->GetValue()));
	}
}

URSAudioSettingsSubsystem* URSAudioSettingsWidget::GetAudioSettingsSubsystem() const
{
	UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<URSAudioSettingsSubsystem>() : nullptr;
}

void URSAudioSettingsWidget::HandleSliderValueChanged(float)
{
	if (!bIsSynchronizingSliders)
	{
		UpdateValueTexts();
		ApplyCurrentSliderValues();

		if (!bIsSliderMouseCaptured)
		{
			SaveAudioSettingsIfNeeded();
		}
	}
}

void URSAudioSettingsWidget::HandleSliderMouseCaptureBegin()
{
	bIsSliderMouseCaptured = true;
}

void URSAudioSettingsWidget::HandleSliderMouseCaptureEnd()
{
	bIsSliderMouseCaptured = false;
	SaveAudioSettingsIfNeeded();
}
