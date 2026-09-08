// Fill out your copyright notice in the Description page of Project Settings.

#include "RSBossResultWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"

void URSBossResultWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	UVerticalBox* ResultContainer = Cast<UVerticalBox>(GetWidgetFromName(TEXT("VerticalBox")));
	if (!ResultContainer || !WidgetTree)
	{
		return;
	}

	UHorizontalBox* ActionContainer = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ActionContainer"));
	RestartButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("RestartButton"));
	MainMenuButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("MainMenuButton"));
	UTextBlock* RestartLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RestartLabel"));
	UTextBlock* MainMenuLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MainMenuLabel"));
	if (!ActionContainer || !RestartButton || !MainMenuButton || !RestartLabel || !MainMenuLabel)
	{
		RestartButton = nullptr;
		MainMenuButton = nullptr;
		return;
	}

	RestartLabel->SetText(NSLOCTEXT("RSBossResult", "Restart", "재시작"));
	MainMenuLabel->SetText(NSLOCTEXT("RSBossResult", "MainMenu", "메인 메뉴"));
	RestartButton->AddChild(RestartLabel);
	MainMenuButton->AddChild(MainMenuLabel);

	if (UHorizontalBoxSlot* RestartSlot = ActionContainer->AddChildToHorizontalBox(RestartButton))
	{
		RestartSlot->SetPadding(FMargin(8.0f));
	}
	if (UHorizontalBoxSlot* MainMenuSlot = ActionContainer->AddChildToHorizontalBox(MainMenuButton))
	{
		MainMenuSlot->SetPadding(FMargin(8.0f));
	}

	ResultContainer->AddChild(ActionContainer);
	RestartButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRestartButtonClicked);
	MainMenuButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleMainMenuButtonClicked);
}

void URSBossResultWidget::SetActionsEnabled(bool bEnabled)
{
	if (RestartButton)
	{
		RestartButton->SetIsEnabled(bEnabled);
	}

	if (MainMenuButton)
	{
		MainMenuButton->SetIsEnabled(bEnabled);
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
