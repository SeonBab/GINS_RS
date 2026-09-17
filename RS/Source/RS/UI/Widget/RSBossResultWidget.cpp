// Fill out your copyright notice in the Description page of Project Settings.

#include "RSBossResultWidget.h"

#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "RSBossEncounter.h"
#include "RSScoreboardSubsystem.h"

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

	if (EditableTextBox_PlayerName)
	{
		EditableTextBox_PlayerName->OnTextChanged.AddUniqueDynamic(this, &ThisClass::HandlePlayerNameTextChanged);
	}

	if (Button_SaveRecord)
	{
		Button_SaveRecord->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSaveRecordButtonClicked);
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

void URSBossResultWidget::PresentResult(ERSBossEncounterResult Result, float RemainingTimeSeconds, int32 HitCount)
{
	bIsClearResult = Result == ERSBossEncounterResult::Clear;
	bHasEditedPlayerName = false;
	bIsRecordInputFinalized = false;
	ResultRemainingTimeSeconds = FMath::Max(RemainingTimeSeconds, 0.0f);
	ResultHitCount = FMath::Max(HitCount, 0);

	if (Panel_ClearRecord)
	{
		Panel_ClearRecord->SetVisibility(bIsClearResult ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	UGameInstance* GameInstance = GetGameInstance();
	URSScoreboardSubsystem* ScoreboardSubsystem = GameInstance ? GameInstance->GetSubsystem<URSScoreboardSubsystem>() : nullptr;

	if (TextBlock_RemainingTime)
	{
		const FString FormattedTime = ScoreboardSubsystem
			? ScoreboardSubsystem->FormatRemainingTime(ScoreboardSubsystem->ConvertRemainingTimeToMilliseconds(ResultRemainingTimeSeconds))
			: RSScoreboard::FormatRemainingTime(RSScoreboard::ConvertRemainingTimeToMilliseconds(ResultRemainingTimeSeconds));
		TextBlock_RemainingTime->SetText(FText::Format(NSLOCTEXT("RSBossResult", "RemainingTime", "남은 시간  {0}"), FText::FromString(FormattedTime)));
	}

	if (TextBlock_HitCount)
	{
		TextBlock_HitCount->SetText(FText::Format(NSLOCTEXT("RSBossResult", "HitCount", "피격  {0}회"), FText::AsNumber(ResultHitCount)));
	}

	bIsUpdatingPlayerName = true;
	if (EditableTextBox_PlayerName)
	{
		EditableTextBox_PlayerName->SetText(FText::GetEmpty());
	}
	bIsUpdatingPlayerName = false;

	const bool bCanSave = bIsClearResult && ScoreboardSubsystem && ScoreboardSubsystem->CanSaveRecords();
	if (EditableTextBox_PlayerName)
	{
		EditableTextBox_PlayerName->SetIsEnabled(bCanSave);
	}
	if (Button_SaveRecord)
	{
		Button_SaveRecord->SetIsEnabled(false);
	}
	if (TextBlock_RecordFeedback)
	{
		const FText InitialMessage = bIsClearResult && ScoreboardSubsystem && !bCanSave
			? GetLoadUnavailableMessage(ScoreboardSubsystem->GetLoadState())
			: FText::GetEmpty();
		TextBlock_RecordFeedback->SetText(InitialMessage);
	}
}

void URSBossResultWidget::RefreshNameValidation(const FText& PlayerName)
{
	if (!bIsClearResult || bIsRecordInputFinalized)
	{
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	URSScoreboardSubsystem* ScoreboardSubsystem = GameInstance ? GameInstance->GetSubsystem<URSScoreboardSubsystem>() : nullptr;
	if (!ScoreboardSubsystem || !ScoreboardSubsystem->CanSaveRecords())
	{
		if (Button_SaveRecord)
		{
			Button_SaveRecord->SetIsEnabled(false);
		}
		return;
	}

	FString NormalizedPlayerName;
	const ERSScoreboardNameValidationResult ValidationResult = ScoreboardSubsystem->NormalizeAndValidatePlayerName(PlayerName.ToString(), NormalizedPlayerName);
	if (Button_SaveRecord)
	{
		Button_SaveRecord->SetIsEnabled(ValidationResult == ERSScoreboardNameValidationResult::Valid);
	}
	if (TextBlock_RecordFeedback)
	{
		TextBlock_RecordFeedback->SetText(bHasEditedPlayerName ? GetNameValidationMessage(ValidationResult) : FText::GetEmpty());
	}
}

FText URSBossResultWidget::GetNameValidationMessage(ERSScoreboardNameValidationResult ValidationResult)
{
	switch (ValidationResult)
	{
	case ERSScoreboardNameValidationResult::Valid:
		return FText::GetEmpty();
	case ERSScoreboardNameValidationResult::Empty:
		return NSLOCTEXT("RSBossResult", "EmptyName", "이름을 입력해 주세요.");
	case ERSScoreboardNameValidationResult::TooLong:
		return NSLOCTEXT("RSBossResult", "LongName", "이름은 12자 이하로 입력해 주세요.");
	case ERSScoreboardNameValidationResult::InvalidCharacter:
		return NSLOCTEXT("RSBossResult", "InvalidNameCharacter", "한글, 영문, 숫자와 일반 공백만 사용할 수 있습니다.");
	default:
		return NSLOCTEXT("RSBossResult", "InvalidName", "사용할 수 없는 이름입니다.");
	}
}

FText URSBossResultWidget::GetLoadUnavailableMessage(ERSScoreboardLoadState LoadState)
{
	if (LoadState == ERSScoreboardLoadState::FutureVersion)
	{
		return NSLOCTEXT("RSBossResult", "FutureVersion", "더 최신 게임 버전에서 만든 스코어보드입니다. 기록 보호를 위해 저장이 비활성화되었습니다.");
	}

	return NSLOCTEXT("RSBossResult", "LoadUnavailable", "스코어보드 기록을 읽을 수 없습니다. 기존 파일 보호를 위해 기록 저장이 비활성화되었습니다.");
}

FText URSBossResultWidget::GetSaveOutcomeMessage(const FRSScoreboardSaveOutcome& SaveOutcome)
{
	switch (SaveOutcome.Result)
	{
	case ERSScoreboardSaveResult::Saved:
		return SaveOutcome.bIsTiedRank
			? FText::Format(NSLOCTEXT("RSBossResult", "SavedTied", "기록을 저장했습니다. 공동 {0}위입니다."), FText::AsNumber(SaveOutcome.DisplayRank))
			: FText::Format(NSLOCTEXT("RSBossResult", "Saved", "기록을 저장했습니다. {0}위입니다."), FText::AsNumber(SaveOutcome.DisplayRank));
	case ERSScoreboardSaveResult::Duplicate:
		return NSLOCTEXT("RSBossResult", "Duplicate", "동일한 기록이 이미 저장되어 있습니다.");
	case ERSScoreboardSaveResult::NotRanked:
		return NSLOCTEXT("RSBossResult", "NotRanked", "순위권 밖으로 기록이 저장되지 않았습니다.");
	case ERSScoreboardSaveResult::LoadUnavailable:
		return NSLOCTEXT("RSBossResult", "SaveLoadUnavailable", "스코어보드 기록을 읽을 수 없어 저장할 수 없습니다.");
	case ERSScoreboardSaveResult::SaveFailed:
		return NSLOCTEXT("RSBossResult", "SaveFailed", "기록을 저장하지 못했습니다. 다시 시도해 주세요.");
	case ERSScoreboardSaveResult::InvalidName:
	default:
		return NSLOCTEXT("RSBossResult", "SaveInvalidName", "사용할 수 없는 이름입니다.");
	}
}

void URSBossResultWidget::HandlePlayerNameTextChanged(const FText& PlayerName)
{
	if (bIsUpdatingPlayerName)
	{
		return;
	}

	bHasEditedPlayerName = true;
	RefreshNameValidation(PlayerName);
}

void URSBossResultWidget::HandleSaveRecordButtonClicked()
{
	if (!bIsClearResult || bIsRecordInputFinalized || !EditableTextBox_PlayerName)
	{
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	URSScoreboardSubsystem* ScoreboardSubsystem = GameInstance ? GameInstance->GetSubsystem<URSScoreboardSubsystem>() : nullptr;
	if (!ScoreboardSubsystem || !ScoreboardSubsystem->CanSaveRecords())
	{
		if (EditableTextBox_PlayerName)
		{
			EditableTextBox_PlayerName->SetIsEnabled(false);
		}
		if (Button_SaveRecord)
		{
			Button_SaveRecord->SetIsEnabled(false);
		}
		if (TextBlock_RecordFeedback)
		{
			TextBlock_RecordFeedback->SetText(ScoreboardSubsystem ? GetLoadUnavailableMessage(ScoreboardSubsystem->GetLoadState()) : GetLoadUnavailableMessage(ERSScoreboardLoadState::CorruptOrUnsupported));
		}
		return;
	}

	FString NormalizedPlayerName;
	const ERSScoreboardNameValidationResult ValidationResult = ScoreboardSubsystem->NormalizeAndValidatePlayerName(EditableTextBox_PlayerName->GetText().ToString(), NormalizedPlayerName);
	if (ValidationResult != ERSScoreboardNameValidationResult::Valid)
	{
		if (TextBlock_RecordFeedback)
		{
			TextBlock_RecordFeedback->SetText(GetNameValidationMessage(ValidationResult));
		}
		return;
	}

	bIsUpdatingPlayerName = true;
	EditableTextBox_PlayerName->SetText(FText::FromString(NormalizedPlayerName));
	bIsUpdatingPlayerName = false;

	const FRSScoreboardSaveOutcome SaveOutcome = ScoreboardSubsystem->TrySaveClearRecord(NormalizedPlayerName, ResultRemainingTimeSeconds, ResultHitCount);
	if (TextBlock_RecordFeedback)
	{
		TextBlock_RecordFeedback->SetText(GetSaveOutcomeMessage(SaveOutcome));
	}

	bIsRecordInputFinalized = SaveOutcome.Result == ERSScoreboardSaveResult::Saved || SaveOutcome.Result == ERSScoreboardSaveResult::NotRanked;
	if (bIsRecordInputFinalized)
	{
		EditableTextBox_PlayerName->SetIsEnabled(false);
		if (Button_SaveRecord)
		{
			Button_SaveRecord->SetIsEnabled(false);
		}
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
