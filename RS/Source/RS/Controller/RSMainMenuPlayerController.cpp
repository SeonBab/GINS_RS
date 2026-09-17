// Fill out your copyright notice in the Description page of Project Settings.

#include "RSMainMenuPlayerController.h"

#include "Blueprint/UserWidget.h"
#include "Kismet/KismetSystemLibrary.h"
#include "RSMainMenuGameMode.h"
#include "RSMainMenuSettingsWidget.h"
#include "RSMainMenuScoreboardWidget.h"
#include "RSMainMenuWidget.h"
#include "RSMusicPlaybackSubsystem.h"
#include "RSQuitConfirmationWidget.h"
#include "RSScreenFadeWidget.h"

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
	CreateScreenFadeWidget();
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

	if (MainMenuScoreboardWidget)
	{
		MainMenuScoreboardWidget->RemoveFromParent();
	}

	if (QuitConfirmationWidget)
	{
		QuitConfirmationWidget->RemoveFromParent();
	}

	if (ScreenFadeWidget)
	{
		ScreenFadeWidget->RemoveFromParent();
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
	MainMenuWidget->GetScoreboardRequested().AddUObject(this, &ThisClass::HandleScoreboardRequested);
	MainMenuWidget->GetAudioSettingsRequested().AddUObject(this, &ThisClass::HandleAudioSettingsRequested);
	MainMenuWidget->GetQuitConfirmationRequested().AddUObject(this, &ThisClass::HandleQuitConfirmationRequested);

	return true;
}

bool ARSMainMenuPlayerController::PlayScreenTransition(const FSimpleDelegate& OnFadedOut)
{
	if (!IsLocalController() || !ScreenFadeWidget || !ScreenFadeWidget->PlayFadeOutThen(OnFadedOut))
	{
		return false;
	}

	// 화면과 음악이 같은 길이로 사라지게 합니다
	if (UWorld* World = GetWorld())
	{
		if (URSMusicPlaybackSubsystem* MusicPlaybackSubsystem = World->GetSubsystem<URSMusicPlaybackSubsystem>())
		{
			MusicPlaybackSubsystem->StopMusic(ScreenFadeWidget->GetFadeOutDuration());
		}
	}

	return true;
}

void ARSMainMenuPlayerController::CreateScreenFadeWidget()
{
	if (ScreenFadeWidget || !ScreenFadeWidgetClass)
	{
		return;
	}

	ScreenFadeWidget = CreateWidget<URSScreenFadeWidget>(this, ScreenFadeWidgetClass);
	if (!ScreenFadeWidget)
	{
		return;
	}

	// 설정과 종료 확인 화면보다 위에서 화면 전체를 덮어야 합니다
	ScreenFadeWidget->AddToViewport(100);
	ScreenFadeWidget->PlayFadeIn();
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

	if (MainMenuScoreboardWidget)
	{
		MainMenuScoreboardWidget->RemoveFromParent();
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

void ARSMainMenuPlayerController::HandleScoreboardRequested()
{
	if (!MainMenuWidget || !MainMenuScoreboardWidgetClass)
	{
		return;
	}

	if (!MainMenuScoreboardWidget)
	{
		MainMenuScoreboardWidget = CreateWidget<URSMainMenuScoreboardWidget>(this, MainMenuScoreboardWidgetClass);
		if (!MainMenuScoreboardWidget)
		{
			return;
		}

		MainMenuScoreboardWidget->GetMainMenuScoreboardClosed().AddUObject(this, &ThisClass::HandleMainMenuScoreboardClosed);
	}

	if (!MainMenuScoreboardWidget->OpenScoreboard())
	{
		return;
	}

	MainMenuWidget->SetVisibility(ESlateVisibility::Collapsed);
	MainMenuScoreboardWidget->AddToViewport(10);
	ConfigureUserInterfaceInput(MainMenuScoreboardWidget);
	MainMenuScoreboardWidget->RequestInitialFocus(this);
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

void ARSMainMenuPlayerController::HandleMainMenuScoreboardClosed()
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

	// 연출을 사용할 수 없으면 기존 동작대로 즉시 종료합니다
	const FSimpleDelegate OnFadedOut = FSimpleDelegate::CreateUObject(this, &ThisClass::QuitApplication);
	if (!PlayScreenTransition(OnFadedOut))
	{
		QuitApplication();
	}
}

void ARSMainMenuPlayerController::QuitApplication()
{
	UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
}
