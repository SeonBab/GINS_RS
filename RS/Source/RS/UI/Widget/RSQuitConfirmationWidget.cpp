// Fill out your copyright notice in the Description page of Project Settings.

#include "RSQuitConfirmationWidget.h"

#include "Components/Button.h"
#include "InputCoreTypes.h"

void URSQuitConfirmationWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (Button_Confirm)
	{
		Button_Confirm->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleConfirmButtonClicked);
	}

	if (Button_Cancel)
	{
		Button_Cancel->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCancelButtonClicked);
	}
}

FReply URSQuitConfirmationWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		HandleCancelButtonClicked();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void URSQuitConfirmationWidget::RequestInitialFocus(APlayerController* PlayerController)
{
	if (Button_Cancel && PlayerController)
	{
		Button_Cancel->SetUserFocus(PlayerController);
	}
}

void URSQuitConfirmationWidget::HandleConfirmButtonClicked()
{
	OnQuitConfirmed.Broadcast();
}

void URSQuitConfirmationWidget::HandleCancelButtonClicked()
{
	OnQuitCancelled.Broadcast();
}

