// Fill out your copyright notice in the Description page of Project Settings.

#include "RSMainMenuSettingsWidget.h"

#include "Components/Button.h"
#include "InputCoreTypes.h"
#include "RSAudioSettingsWidget.h"

void URSMainMenuSettingsWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (Button_Close)
	{
		Button_Close->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCloseButtonClicked);
	}
}

FReply URSMainMenuSettingsWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		HandleCloseButtonClicked();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

bool URSMainMenuSettingsWidget::OpenSettings()
{
	return Widget_AudioSettings && Widget_AudioSettings->OpenAudioSettings();
}

void URSMainMenuSettingsWidget::RequestInitialFocus(APlayerController* PlayerController)
{
	if (Widget_AudioSettings)
	{
		Widget_AudioSettings->RequestInitialFocus(PlayerController);
	}
}

void URSMainMenuSettingsWidget::HandleCloseButtonClicked()
{
	OnMainMenuSettingsClosed.Broadcast();
}
