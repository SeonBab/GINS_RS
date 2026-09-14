// Fill out your copyright notice in the Description page of Project Settings.

#include "RSMainMenuWidget.h"

#include "Components/Button.h"

void URSMainMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (Button_StartGame)
	{
		Button_StartGame->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleStartGameButtonClicked);
	}

	if (Button_AudioSettings)
	{
		Button_AudioSettings->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleAudioSettingsButtonClicked);
	}

	if (Button_QuitGame)
	{
		Button_QuitGame->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleQuitGameButtonClicked);
	}
}

void URSMainMenuWidget::SetActionsEnabled(bool bEnabled)
{
	if (Button_StartGame)
	{
		Button_StartGame->SetIsEnabled(bEnabled);
	}

	if (Button_AudioSettings)
	{
		Button_AudioSettings->SetIsEnabled(bEnabled);
	}

	if (Button_QuitGame)
	{
		Button_QuitGame->SetIsEnabled(bEnabled);
	}
}

void URSMainMenuWidget::RequestInitialFocus(APlayerController* PlayerController)
{
	if (Button_StartGame && PlayerController)
	{
		Button_StartGame->SetUserFocus(PlayerController);
	}
}

void URSMainMenuWidget::HandleStartGameButtonClicked()
{
	OnStartGameRequested.Broadcast();
}

void URSMainMenuWidget::HandleAudioSettingsButtonClicked()
{
	OnAudioSettingsRequested.Broadcast();
}

void URSMainMenuWidget::HandleQuitGameButtonClicked()
{
	OnQuitConfirmationRequested.Broadcast();
}

