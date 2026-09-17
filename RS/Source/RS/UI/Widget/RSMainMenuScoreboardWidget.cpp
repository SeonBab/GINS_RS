#include "RSMainMenuScoreboardWidget.h"

#include "Components/Button.h"
#include "InputCoreTypes.h"
#include "RSScoreboardPanelWidget.h"

void URSMainMenuScoreboardWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (Button_Close)
	{
		Button_Close->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCloseButtonClicked);
	}
}

void URSMainMenuScoreboardWidget::NativeDestruct()
{
	if (Widget_Scoreboard)
	{
		Widget_Scoreboard->CloseScoreboard();
	}

	Super::NativeDestruct();
}

FReply URSMainMenuScoreboardWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		HandleCloseButtonClicked();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

bool URSMainMenuScoreboardWidget::OpenScoreboard()
{
	return Widget_Scoreboard && Widget_Scoreboard->OpenScoreboard();
}

void URSMainMenuScoreboardWidget::RequestInitialFocus(APlayerController* PlayerController)
{
	if (Button_Close && PlayerController)
	{
		Button_Close->SetUserFocus(PlayerController);
	}
}

void URSMainMenuScoreboardWidget::HandleCloseButtonClicked()
{
	if (Widget_Scoreboard)
	{
		Widget_Scoreboard->CloseScoreboard();
	}

	OnMainMenuScoreboardClosed.Broadcast();
}
