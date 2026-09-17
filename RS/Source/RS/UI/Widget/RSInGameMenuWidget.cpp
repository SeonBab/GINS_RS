// Fill out your copyright notice in the Description page of Project Settings.

#include "RSInGameMenuWidget.h"

#include "Components/Button.h"
#include "Components/WidgetSwitcher.h"
#include "InputCoreTypes.h"
#include "RSAudioSettingsWidget.h"
#include "RSScoreboardPanelWidget.h"

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

	if (Button_Scoreboard)
	{
		Button_Scoreboard->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleScoreboardButtonClicked);
	}

	if (Button_Settings)
	{
		Button_Settings->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSettingsButtonClicked);
	}

	if (Button_BackFromSettings)
	{
		Button_BackFromSettings->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleBackButtonClicked);
	}

	if (Button_BackFromScoreboard)
	{
		Button_BackFromScoreboard->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleBackButtonClicked);
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
		if (WidgetSwitcher_MenuPages && WidgetSwitcher_MenuPages->GetActiveWidget() != Panel_ActionPage)
		{
			HandleBackButtonClicked();
		}
		else
		{
			OnContinueRequested.Broadcast();
		}
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void URSInGameMenuWidget::NativeDestruct()
{
	if (Widget_Scoreboard)
	{
		Widget_Scoreboard->CloseScoreboard();
	}

	Super::NativeDestruct();
}

bool URSInGameMenuWidget::OpenMenu()
{
	if (!WidgetSwitcher_MenuPages || !Panel_ActionPage)
	{
		return false;
	}

	if (Widget_Scoreboard)
	{
		Widget_Scoreboard->CloseScoreboard();
	}

	WidgetSwitcher_MenuPages->SetActiveWidget(Panel_ActionPage);
	return true;
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

	if (Button_Scoreboard)
	{
		Button_Scoreboard->SetIsEnabled(bEnabled);
	}

	if (Button_Settings)
	{
		Button_Settings->SetIsEnabled(bEnabled);
	}

	if (Button_MainMenu)
	{
		Button_MainMenu->SetIsEnabled(bEnabled);
	}
}

void URSInGameMenuWidget::HandleScoreboardButtonClicked()
{
	if (!WidgetSwitcher_MenuPages || !Panel_ScoreboardPage || !Widget_Scoreboard || !Widget_Scoreboard->OpenScoreboard())
	{
		return;
	}

	WidgetSwitcher_MenuPages->SetActiveWidget(Panel_ScoreboardPage);
	if (Button_BackFromScoreboard && GetOwningPlayer())
	{
		Button_BackFromScoreboard->SetUserFocus(GetOwningPlayer());
	}
}

void URSInGameMenuWidget::HandleSettingsButtonClicked()
{
	if (!WidgetSwitcher_MenuPages || !Panel_SettingsPage || !Widget_AudioSettings || !Widget_AudioSettings->OpenAudioSettings())
	{
		return;
	}

	WidgetSwitcher_MenuPages->SetActiveWidget(Panel_SettingsPage);
	Widget_AudioSettings->RequestInitialFocus(GetOwningPlayer());
}

void URSInGameMenuWidget::HandleBackButtonClicked()
{
	if (!WidgetSwitcher_MenuPages || !Panel_ActionPage)
	{
		return;
	}

	if (Widget_Scoreboard)
	{
		Widget_Scoreboard->CloseScoreboard();
	}

	WidgetSwitcher_MenuPages->SetActiveWidget(Panel_ActionPage);
	RequestInitialFocus(GetOwningPlayer());
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
