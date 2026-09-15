// Fill out your copyright notice in the Description page of Project Settings.

#include "RSInGameMenuWidget.h"

#include "Components/Button.h"
#include "InputCoreTypes.h"
#include "RSAudioSettingsWidget.h"

void URSInGameMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (Button_Continue)
	{
		Button_Continue->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleContinueButtonClicked);
	}

	if (Button_Restart)
	{
		Button_Restart->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRestartButtonClicked);
	}

	if (Button_MainMenu)
	{
		Button_MainMenu->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleMainMenuButtonClicked);
	}
}

FReply URSInGameMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		OnContinueRequested.Broadcast();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

bool URSInGameMenuWidget::OpenMenu()
{
	return Widget_AudioSettings && Widget_AudioSettings->OpenAudioSettings();
}

void URSInGameMenuWidget::RequestInitialFocus(APlayerController* PlayerController)
{
	if (Button_Continue && PlayerController)
	{
		Button_Continue->SetUserFocus(PlayerController);
	}
}

void URSInGameMenuWidget::SetActionsEnabled(bool bEnabled)
{
	if (Button_Continue)
	{
		Button_Continue->SetIsEnabled(bEnabled);
	}

	if (Button_Restart)
	{
		Button_Restart->SetIsEnabled(bEnabled);
	}

	if (Button_MainMenu)
	{
		Button_MainMenu->SetIsEnabled(bEnabled);
	}
}

void URSInGameMenuWidget::HandleContinueButtonClicked()
{
	OnContinueRequested.Broadcast();
}

void URSInGameMenuWidget::HandleRestartButtonClicked()
{
	OnRestartRequested.Broadcast();
}

void URSInGameMenuWidget::HandleMainMenuButtonClicked()
{
	OnMainMenuRequested.Broadcast();
}
