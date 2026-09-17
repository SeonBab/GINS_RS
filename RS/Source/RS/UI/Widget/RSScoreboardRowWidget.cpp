#include "RSScoreboardRowWidget.h"

#include "Components/TextBlock.h"

void URSScoreboardRowWidget::SetRecord(int32 DisplayRank, const FRSBossClearRecord& Record)
{
	if (TextBlock_Rank)
	{
		TextBlock_Rank->SetText(FText::AsNumber(FMath::Max(DisplayRank, 1)));
	}

	if (TextBlock_PlayerName)
	{
		TextBlock_PlayerName->SetText(FText::FromString(Record.PlayerName));
	}

	if (TextBlock_RemainingTime)
	{
		TextBlock_RemainingTime->SetText(FText::FromString(RSScoreboard::FormatRemainingTime(Record.RemainingTimeMilliseconds)));
	}

	if (TextBlock_HitCount)
	{
		TextBlock_HitCount->SetText(FText::Format(NSLOCTEXT("RSScoreboard", "HitCount", "{0}회"), FText::AsNumber(FMath::Max(Record.HitCount, 0))));
	}
}
