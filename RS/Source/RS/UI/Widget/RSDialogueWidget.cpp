// Fill out your copyright notice in the Description page of Project Settings.

#include "RSDialogueWidget.h"

#include "Components/TextBlock.h"
#include "InputCoreTypes.h"

URSDialogueWidget::URSDialogueWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void URSDialogueWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	HideDialogue();
}

FReply URSDialogueWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::G || Key == EKeys::Escape)
	{
		RequestClose();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply URSDialogueWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FKey MouseButton = InMouseEvent.GetEffectingButton();
	if (MouseButton == EKeys::LeftMouseButton || MouseButton == EKeys::RightMouseButton)
	{
		RequestClose();
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

bool URSDialogueWidget::ShowDialogue(const FText& InDialogueText)
{
	if (!TextBlock_DialogueContent)
	{
		return false;
	}

	TextBlock_DialogueContent->SetText(InDialogueText);
	SetVisibility(ESlateVisibility::Visible);
	return true;
}

void URSDialogueWidget::HideDialogue()
{
	SetVisibility(ESlateVisibility::Collapsed);
}

void URSDialogueWidget::RequestInitialFocus(APlayerController* PlayerController)
{
	if (PlayerController)
	{
		SetUserFocus(PlayerController);
	}
}

void URSDialogueWidget::RequestClose()
{
	OnCloseRequested.Broadcast();
}
