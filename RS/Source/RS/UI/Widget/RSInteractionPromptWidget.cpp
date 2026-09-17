#include "RSInteractionPromptWidget.h"

#include "Components/TextBlock.h"

void URSInteractionPromptWidget::SetPromptText(const FText& InPromptText)
{
	if (!PromptTextBlock)
	{
		return;
	}

	PromptTextBlock->SetText(InPromptText);
}
