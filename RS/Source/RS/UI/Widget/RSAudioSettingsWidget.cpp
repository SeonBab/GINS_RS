// Fill out your copyright notice in the Description page of Project Settings.

#include "RSAudioSettingsWidget.h"

#include "Components/Button.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "InputCoreTypes.h"
#include "RSAudioSettingsSubsystem.h"

void URSAudioSettingsWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (Slider_Master)
	{
		Slider_Master->OnValueChanged.AddUniqueDynamic(this, &ThisClass::HandleSliderValueChanged);
	}

	if (Slider_BackgroundMusic)
	{
		Slider_BackgroundMusic->OnValueChanged.AddUniqueDynamic(this, &ThisClass::HandleSliderValueChanged);
	}

	if (Slider_SoundEffects)
	{
		Slider_SoundEffects->OnValueChanged.AddUniqueDynamic(this, &ThisClass::HandleSliderValueChanged);
	}

	if (Button_Apply)
	{
		Button_Apply->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleApplyButtonClicked);
	}

	if (Button_Cancel)
	{
		Button_Cancel->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCancelButtonClicked);
	}

	if (Button_Defaults)
	{
		Button_Defaults->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleDefaultsButtonClicked);
	}
}

void URSAudioSettingsWidget::NativeDestruct()
{
	if (URSAudioSettingsSubsystem* AudioSettingsSubsystem = GetAudioSettingsSubsystem())
	{
		if (AudioSettingsSubsystem->IsEditingAudioSettings())
		{
			AudioSettingsSubsystem->CancelAudioSettingsEdit();
		}
	}

	Super::NativeDestruct();
}

FReply URSAudioSettingsWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		HandleCancelButtonClicked();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

bool URSAudioSettingsWidget::OpenAudioSettings()
{
	URSAudioSettingsSubsystem* AudioSettingsSubsystem = GetAudioSettingsSubsystem();
	if (!AudioSettingsSubsystem)
	{
		return false;
	}

	FRSAudioVolumeSettings EditingSettings;
	if (!AudioSettingsSubsystem->BeginAudioSettingsEdit(EditingSettings))
	{
		return false;
	}

	SynchronizeSliders(EditingSettings);

	return true;
}

void URSAudioSettingsWidget::RequestInitialFocus(APlayerController* PlayerController)
{
	if (Slider_Master && PlayerController)
	{
		Slider_Master->SetUserFocus(PlayerController);
	}
}

void URSAudioSettingsWidget::PreviewCurrentSliderValues()
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
		AudioSettingsSubsystem->PreviewAudioSettings(Settings);
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
		PreviewCurrentSliderValues();
	}
}

void URSAudioSettingsWidget::HandleApplyButtonClicked()
{
	if (URSAudioSettingsSubsystem* AudioSettingsSubsystem = GetAudioSettingsSubsystem())
	{
		if (AudioSettingsSubsystem->ApplyAudioSettingsEdit())
		{
			OnAudioSettingsClosed.Broadcast();
		}
	}
}

void URSAudioSettingsWidget::HandleCancelButtonClicked()
{
	if (URSAudioSettingsSubsystem* AudioSettingsSubsystem = GetAudioSettingsSubsystem())
	{
		if (AudioSettingsSubsystem->CancelAudioSettingsEdit())
		{
			OnAudioSettingsClosed.Broadcast();
		}
	}
}

void URSAudioSettingsWidget::HandleDefaultsButtonClicked()
{
	if (URSAudioSettingsSubsystem* AudioSettingsSubsystem = GetAudioSettingsSubsystem())
	{
		FRSAudioVolumeSettings DefaultSettings;
		if (AudioSettingsSubsystem->PreviewDefaultAudioSettings(DefaultSettings))
		{
			SynchronizeSliders(DefaultSettings);
		}
	}
}
