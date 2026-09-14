// Fill out your copyright notice in the Description page of Project Settings.

#include "RSMainMenuPlayerController.h"

#include "Blueprint/UserWidget.h"
#include "Kismet/KismetSystemLibrary.h"
#include "RSMainMenuGameMode.h"
#include "RSMainMenuSettingsWidget.h"
#include "RSMainMenuWidget.h"
#include "RSQuitConfirmationWidget.h"

void ARSMainMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController() || !CreateMainMenuWidget())
	{
		return;
	}

	bShowMouseCursor = true;
	MainMenuWidget->AddToViewport();
	ConfigureUserInterfaceInput(MainMenuWidget);
	MainMenuWidget->RequestInitialFocus(this);
}

void ARSMainMenuPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (MainMenuWidget)
	{
		MainMenuWidget->RemoveFromParent();
	}

	if (MainMenuSettingsWidget)
	{
		MainMenuSettingsWidget->RemoveFromParent();
	}

	if (QuitConfirmationWidget)
	{
		QuitConfirmationWidget->RemoveFromParent();
	}

	Super::EndPlay(EndPlayReason);
}

bool ARSMainMenuPlayerController::CreateMainMenuWidget()
{
	if (MainMenuWidget)
	{
		return true;
	}

	if (!MainMenuWidgetClass)
	{
		return false;
	}

	MainMenuWidget = CreateWidget<URSMainMenuWidget>(this, MainMenuWidgetClass);
	if (!MainMenuWidget)
	{
		return false;
	}

	MainMenuWidget->GetStartGameRequested().AddUObject(this, &ThisClass::HandleStartGameRequested);
	MainMenuWidget->GetAudioSettingsRequested().AddUObject(this, &ThisClass::HandleAudioSettingsRequested);
	MainMenuWidget->GetQuitConfirmationRequested().AddUObject(this, &ThisClass::HandleQuitConfirmationRequested);

	return true;
}

void ARSMainMenuPlayerController::ConfigureUserInterfaceInput(UUserWidget* FocusWidget)
{
	if (!FocusWidget)
	{
		return;
	}

	FInputModeUIOnly InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetWidgetToFocus(FocusWidget->TakeWidget());
	SetInputMode(InputMode);
}

void ARSMainMenuPlayerController::RestoreMainMenu()
{
	if (!MainMenuWidget)
	{
		return;
	}

	if (MainMenuSettingsWidget)
	{
		MainMenuSettingsWidget->RemoveFromParent();
	}

	if (QuitConfirmationWidget)
	{
		QuitConfirmationWidget->RemoveFromParent();
	}

	MainMenuWidget->SetVisibility(ESlateVisibility::Visible);
	MainMenuWidget->SetActionsEnabled(true);
	ConfigureUserInterfaceInput(MainMenuWidget);
	MainMenuWidget->RequestInitialFocus(this);
}

void ARSMainMenuPlayerController::HandleStartGameRequested()
{
	if (!MainMenuWidget)
	{
		return;
	}

	MainMenuWidget->SetActionsEnabled(false);

	UWorld* World = GetWorld();
	ARSMainMenuGameMode* GameMode = World ? World->GetAuthGameMode<ARSMainMenuGameMode>() : nullptr;
	if (!GameMode || !GameMode->RequestStartGame(this))
	{
		MainMenuWidget->SetActionsEnabled(true);
	}
}

void ARSMainMenuPlayerController::HandleAudioSettingsRequested()
{
	if (!MainMenuWidget || !MainMenuSettingsWidgetClass)
	{
		return;
	}

	if (!MainMenuSettingsWidget)
	{
		MainMenuSettingsWidget = CreateWidget<URSMainMenuSettingsWidget>(this, MainMenuSettingsWidgetClass);
		if (!MainMenuSettingsWidget)
		{
			return;
		}

		MainMenuSettingsWidget->GetMainMenuSettingsClosed().AddUObject(this, &ThisClass::HandleMainMenuSettingsClosed);
	}

	if (!MainMenuSettingsWidget->OpenSettings())
	{
		return;
	}

	MainMenuWidget->SetVisibility(ESlateVisibility::Collapsed);
	MainMenuSettingsWidget->AddToViewport(10);
	ConfigureUserInterfaceInput(MainMenuSettingsWidget);
	MainMenuSettingsWidget->RequestInitialFocus(this);
}

void ARSMainMenuPlayerController::HandleQuitConfirmationRequested()
{
	if (!MainMenuWidget || !QuitConfirmationWidgetClass)
	{
		return;
	}

	if (!QuitConfirmationWidget)
	{
		QuitConfirmationWidget = CreateWidget<URSQuitConfirmationWidget>(this, QuitConfirmationWidgetClass);
		if (!QuitConfirmationWidget)
		{
			return;
		}

		QuitConfirmationWidget->GetQuitConfirmed().AddUObject(this, &ThisClass::HandleQuitConfirmed);
		QuitConfirmationWidget->GetQuitCancelled().AddUObject(this, &ThisClass::HandleQuitCancelled);
	}

	MainMenuWidget->SetVisibility(ESlateVisibility::Collapsed);
	QuitConfirmationWidget->AddToViewport(10);
	ConfigureUserInterfaceInput(QuitConfirmationWidget);
	QuitConfirmationWidget->RequestInitialFocus(this);
}

void ARSMainMenuPlayerController::HandleMainMenuSettingsClosed()
{
	RestoreMainMenu();
}

void ARSMainMenuPlayerController::HandleQuitCancelled()
{
	RestoreMainMenu();
}

void ARSMainMenuPlayerController::HandleQuitConfirmed()
{
	if (MainMenuWidget)
	{
		MainMenuWidget->SetActionsEnabled(false);
	}

	if (QuitConfirmationWidget)
	{
		QuitConfirmationWidget->RemoveFromParent();
	}

	UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
}
