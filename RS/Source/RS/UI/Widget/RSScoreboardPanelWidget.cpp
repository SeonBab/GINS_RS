#include "RSScoreboardPanelWidget.h"

#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "RSScoreboardRowWidget.h"
#include "RSScoreboardSubsystem.h"

void URSScoreboardPanelWidget::NativeDestruct()
{
	CloseScoreboard();

	Super::NativeDestruct();
}

bool URSScoreboardPanelWidget::OpenScoreboard()
{
	URSScoreboardSubsystem* ScoreboardSubsystem = GetScoreboardSubsystem();
	if (!ScoreboardSubsystem || !ScrollBox_RecordList || !TextBlock_LoadStatus || !TextBlock_EmptyMessage || !ScoreboardRowWidgetClass)
	{
		return false;
	}

	if (BoundScoreboardSubsystem != ScoreboardSubsystem)
	{
		CloseScoreboard();
		BoundScoreboardSubsystem = ScoreboardSubsystem;
		BoundScoreboardSubsystem->OnScoreboardChanged.AddUniqueDynamic(this, &ThisClass::HandleScoreboardChanged);
	}

	RefreshScoreboard();
	return true;
}

void URSScoreboardPanelWidget::CloseScoreboard()
{
	if (BoundScoreboardSubsystem)
	{
		BoundScoreboardSubsystem->OnScoreboardChanged.RemoveDynamic(this, &ThisClass::HandleScoreboardChanged);
		BoundScoreboardSubsystem = nullptr;
	}
}

void URSScoreboardPanelWidget::HandleScoreboardChanged()
{
	RefreshScoreboard();
}

void URSScoreboardPanelWidget::RefreshScoreboard()
{
	if (!BoundScoreboardSubsystem || !ScrollBox_RecordList || !TextBlock_LoadStatus || !TextBlock_EmptyMessage || !ScoreboardRowWidgetClass)
	{
		return;
	}

	ScrollBox_RecordList->ClearChildren();
	const TArray<FRSBossClearRecord> Records = BoundScoreboardSubsystem->GetRecords();
	for (int32 RecordIndex = 0; RecordIndex < Records.Num(); ++RecordIndex)
	{
		URSScoreboardRowWidget* RecordRowWidget = CreateWidget<URSScoreboardRowWidget>(this, ScoreboardRowWidgetClass);
		if (!RecordRowWidget)
		{
			continue;
		}

		bool bIsTiedRank = false;
		const int32 DisplayRank = RSScoreboard::GetDisplayRank(Records, RecordIndex, bIsTiedRank);
		RecordRowWidget->SetRecord(DisplayRank, Records[RecordIndex]);
		ScrollBox_RecordList->AddChild(RecordRowWidget);
	}

	FText LoadStatusText;
	const ERSScoreboardLoadState LoadState = BoundScoreboardSubsystem->GetLoadState();
	switch (LoadState)
	{
	case ERSScoreboardLoadState::RecoveredFromRedundantSlot:
		LoadStatusText = NSLOCTEXT("RSScoreboard", "RecoveredFromRedundantSlot", "일부 저장 파일을 읽지 못해 유효한 백업 기록을 사용했습니다.");
		break;
	case ERSScoreboardLoadState::CorruptOrUnsupported:
		LoadStatusText = NSLOCTEXT("RSScoreboard", "CorruptOrUnsupported", "스코어보드 기록을 읽을 수 없습니다. 기존 파일 보호를 위해 기록 저장이 비활성화되었습니다.");
		break;
	case ERSScoreboardLoadState::FutureVersion:
		LoadStatusText = NSLOCTEXT("RSScoreboard", "FutureVersion", "더 최신 게임 버전에서 만든 스코어보드입니다. 기록 보호를 위해 저장이 비활성화되었습니다.");
		break;
	default:
		break;
	}

	const bool bHasLoadStatus = !LoadStatusText.IsEmpty();
	TextBlock_LoadStatus->SetText(LoadStatusText);
	TextBlock_LoadStatus->SetVisibility(bHasLoadStatus ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	const bool bLoadUnavailable = LoadState == ERSScoreboardLoadState::CorruptOrUnsupported || LoadState == ERSScoreboardLoadState::FutureVersion;
	const bool bShowEmptyMessage = Records.IsEmpty() && !bLoadUnavailable;
	TextBlock_EmptyMessage->SetText(NSLOCTEXT("RSScoreboard", "EmptyRecords", "저장된 클리어 기록이 없습니다."));
	TextBlock_EmptyMessage->SetVisibility(bShowEmptyMessage ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	ScrollBox_RecordList->SetVisibility(Records.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
}

URSScoreboardSubsystem* URSScoreboardPanelWidget::GetScoreboardSubsystem() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<URSScoreboardSubsystem>() : nullptr;
}
