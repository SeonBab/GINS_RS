// Fill out your copyright notice in the Description page of Project Settings.

#include "RSBossResultWidget.h"

#include "Components/Button.h"

void URSBossResultWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (Button_Restart)
	{
		Button_Restart->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRestartButtonClicked);
	}

	if (Button_MainMenu)
	{
		Button_MainMenu->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleMainMenuButtonClicked);
	}
}

void URSBossResultWidget::SetActionsEnabled(bool bEnabled)
{
	if (Button_Restart)
	{
		Button_Restart->SetIsEnabled(bEnabled);
	}

	if (Button_MainMenu)
	{
		Button_MainMenu->SetIsEnabled(bEnabled);
	}
}

void URSBossResultWidget::HandleRestartButtonClicked()
{
	OnBossResultActionRequested.Broadcast(ERSBossResultAction::RestartLevel);
}

void URSBossResultWidget::HandleMainMenuButtonClicked()
{
	OnBossResultActionRequested.Broadcast(ERSBossResultAction::ReturnToMainMenu);
}
