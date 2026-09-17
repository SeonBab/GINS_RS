#include "RSScoreboardSubsystem.h"

#include "Kismet/GameplayStatics.h"
#include "RSScoreboardSaveGame.h"

DEFINE_LOG_CATEGORY_STATIC(LogRSScoreboard, Log, All);

namespace
{
	constexpr int32 ScoreboardSlotCount = 3;

	struct FRSLoadedScoreboardSlot
	{
		int32 SlotIndex = INDEX_NONE;
		int64 Generation = 0;
		TArray<FRSBossClearRecord> Records;
	};
}

void URSScoreboardSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadScoreboard();
}

void URSScoreboardSubsystem::Deinitialize()
{
	OnScoreboardChanged.Clear();
	Records.Reset();
	InvalidSlotIndices.Reset();
	LoadState = ERSScoreboardLoadState::Empty;
	CurrentGeneration = 0;
	ActiveSlotIndex = INDEX_NONE;

	Super::Deinitialize();
}

bool URSScoreboardSubsystem::CanSaveRecords() const
{
	return LoadState == ERSScoreboardLoadState::Empty
		|| LoadState == ERSScoreboardLoadState::Loaded
		|| LoadState == ERSScoreboardLoadState::RecoveredFromRedundantSlot;
}

ERSScoreboardNameValidationResult URSScoreboardSubsystem::NormalizeAndValidatePlayerName(const FString& PlayerName, FString& NormalizedPlayerName) const
{
	return RSScoreboard::NormalizeAndValidatePlayerName(PlayerName, NormalizedPlayerName);
}

int32 URSScoreboardSubsystem::ConvertRemainingTimeToMilliseconds(float RemainingTimeSeconds) const
{
	return RSScoreboard::ConvertRemainingTimeToMilliseconds(RemainingTimeSeconds);
}

FString URSScoreboardSubsystem::FormatRemainingTime(int32 RemainingTimeMilliseconds) const
{
	return RSScoreboard::FormatRemainingTime(RemainingTimeMilliseconds);
}

FRSScoreboardSaveOutcome URSScoreboardSubsystem::TrySaveClearRecord(const FString& PlayerName, float RemainingTimeSeconds, int32 HitCount)
{
	FRSScoreboardSaveOutcome Outcome;
	if (!CanSaveRecords())
	{
		Outcome.Result = ERSScoreboardSaveResult::LoadUnavailable;
		return Outcome;
	}

	FRSBossClearRecord NewRecord;
	if (RSScoreboard::NormalizeAndValidatePlayerName(PlayerName, NewRecord.PlayerName) != ERSScoreboardNameValidationResult::Valid)
	{
		Outcome.Result = ERSScoreboardSaveResult::InvalidName;
		return Outcome;
	}

	NewRecord.RemainingTimeMilliseconds = RSScoreboard::ConvertRemainingTimeToMilliseconds(RemainingTimeSeconds);
	NewRecord.HitCount = FMath::Max(0, HitCount);

	TArray<FRSBossClearRecord> CandidateRecords;
	Outcome = BuildSaveCandidate(Records, NewRecord, CandidateRecords);
	if (Outcome.Result != ERSScoreboardSaveResult::Saved)
	{
		return Outcome;
	}

	if (CurrentGeneration == MAX_int64)
	{
		UE_LOG(LogRSScoreboard, Error, TEXT("Scoreboard generation reached its maximum value"));
		Outcome.Result = ERSScoreboardSaveResult::SaveFailed;
		Outcome.DisplayRank = 0;
		Outcome.bIsTiedRank = false;
		return Outcome;
	}

	const int64 NewGeneration = CurrentGeneration + 1;
	const int32 TargetSlotIndex = static_cast<int32>((NewGeneration - 1) % ScoreboardSlotCount);
	URSScoreboardSaveGame* SaveGame = NewObject<URSScoreboardSaveGame>(this);
	SaveGame->DataVersion = URSScoreboardSaveGame::CurrentDataVersion;
	SaveGame->Generation = NewGeneration;
	SaveGame->Records = CandidateRecords;

	if (!SaveToSlot(SaveGame, TargetSlotIndex))
	{
		UE_LOG(LogRSScoreboard, Error, TEXT("Failed to save scoreboard slot %s for generation %lld"), *GetSlotName(TargetSlotIndex), NewGeneration);
		Outcome.Result = ERSScoreboardSaveResult::SaveFailed;
		Outcome.DisplayRank = 0;
		Outcome.bIsTiedRank = false;
		return Outcome;
	}

	Records = MoveTemp(CandidateRecords);
	CurrentGeneration = NewGeneration;
	ActiveSlotIndex = TargetSlotIndex;
	InvalidSlotIndices.Remove(TargetSlotIndex);
	LoadState = InvalidSlotIndices.IsEmpty() ? ERSScoreboardLoadState::Loaded : ERSScoreboardLoadState::RecoveredFromRedundantSlot;
	OnScoreboardChanged.Broadcast();
	return Outcome;
}

#if WITH_DEV_AUTOMATION_TESTS
void URSScoreboardSubsystem::InitializeForAutomationTest(const FString& InSlotPrefix)
{
	SlotPrefix = InSlotPrefix;
	bForceSaveFailureForAutomationTest = false;
	LoadScoreboard();
}

void URSScoreboardSubsystem::ReloadForAutomationTest()
{
	LoadScoreboard();
}

FString URSScoreboardSubsystem::GetSlotNameForAutomationTest(int32 SlotIndex) const
{
	return GetSlotName(SlotIndex);
}

FRSScoreboardSaveOutcome URSScoreboardSubsystem::BuildSaveCandidateForAutomationTest(const TArray<FRSBossClearRecord>& CurrentRecords, const FRSBossClearRecord& NewRecord, TArray<FRSBossClearRecord>& OutCandidateRecords)
{
	return BuildSaveCandidate(CurrentRecords, NewRecord, OutCandidateRecords);
}
#endif

void URSScoreboardSubsystem::LoadScoreboard()
{
	Records.Reset();
	InvalidSlotIndices.Reset();
	LoadState = ERSScoreboardLoadState::Empty;
	CurrentGeneration = 0;
	ActiveSlotIndex = INDEX_NONE;

	bool bAnySlotExists = false;
	bool bFoundFutureVersion = false;
	TArray<FRSLoadedScoreboardSlot> ValidSlots;

	for (int32 SlotIndex = 0; SlotIndex < ScoreboardSlotCount; ++SlotIndex)
	{
		const FString SlotName = GetSlotName(SlotIndex);
		if (!UGameplayStatics::DoesSaveGameExist(SlotName, 0))
		{
			continue;
		}

		bAnySlotExists = true;
		URSScoreboardSaveGame* SaveGame = Cast<URSScoreboardSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
		if (!SaveGame)
		{
			InvalidSlotIndices.Add(SlotIndex);
			UE_LOG(LogRSScoreboard, Warning, TEXT("Scoreboard slot %s could not be loaded as URSScoreboardSaveGame"), *SlotName);
			continue;
		}

		if (SaveGame->DataVersion > URSScoreboardSaveGame::CurrentDataVersion)
		{
			bFoundFutureVersion = true;
			UE_LOG(LogRSScoreboard, Warning, TEXT("Scoreboard slot %s uses future data version %d"), *SlotName, SaveGame->DataVersion);
			continue;
		}

		if (!IsSaveGameValid(SaveGame, SlotIndex))
		{
			InvalidSlotIndices.Add(SlotIndex);
			continue;
		}

		FRSLoadedScoreboardSlot& LoadedSlot = ValidSlots.AddDefaulted_GetRef();
		LoadedSlot.SlotIndex = SlotIndex;
		LoadedSlot.Generation = SaveGame->Generation;
		LoadedSlot.Records = SaveGame->Records;
	}

	if (bFoundFutureVersion)
	{
		Records.Reset();
		InvalidSlotIndices.Reset();
		LoadState = ERSScoreboardLoadState::FutureVersion;
		return;
	}

	for (int32 FirstIndex = 0; FirstIndex < ValidSlots.Num(); ++FirstIndex)
	{
		for (int32 SecondIndex = FirstIndex + 1; SecondIndex < ValidSlots.Num(); ++SecondIndex)
		{
			if (ValidSlots[FirstIndex].Generation == ValidSlots[SecondIndex].Generation
				&& !AreRecordArraysEqual(ValidSlots[FirstIndex].Records, ValidSlots[SecondIndex].Records))
			{
				UE_LOG(LogRSScoreboard, Error, TEXT("Scoreboard slots %s and %s disagree at generation %lld"), *GetSlotName(ValidSlots[FirstIndex].SlotIndex), *GetSlotName(ValidSlots[SecondIndex].SlotIndex), ValidSlots[FirstIndex].Generation);
				Records.Reset();
				InvalidSlotIndices.Reset();
				LoadState = ERSScoreboardLoadState::CorruptOrUnsupported;
				return;
			}
		}
	}

	if (ValidSlots.IsEmpty())
	{
		LoadState = bAnySlotExists ? ERSScoreboardLoadState::CorruptOrUnsupported : ERSScoreboardLoadState::Empty;
		return;
	}

	const FRSLoadedScoreboardSlot* SelectedSlot = &ValidSlots[0];
	for (const FRSLoadedScoreboardSlot& ValidSlot : ValidSlots)
	{
		if (ValidSlot.Generation > SelectedSlot->Generation
			|| (ValidSlot.Generation == SelectedSlot->Generation && ValidSlot.SlotIndex < SelectedSlot->SlotIndex))
		{
			SelectedSlot = &ValidSlot;
		}
	}

	Records = SelectedSlot->Records;
	CurrentGeneration = SelectedSlot->Generation;
	ActiveSlotIndex = SelectedSlot->SlotIndex;
	LoadState = InvalidSlotIndices.IsEmpty() ? ERSScoreboardLoadState::Loaded : ERSScoreboardLoadState::RecoveredFromRedundantSlot;

	UE_LOG(LogRSScoreboard, Log, TEXT("Loaded %d scoreboard records from slot %s at generation %lld"), Records.Num(), *GetSlotName(ActiveSlotIndex), CurrentGeneration);
}

bool URSScoreboardSubsystem::IsSaveGameValid(const URSScoreboardSaveGame* SaveGame, int32 SlotIndex) const
{
	if (!SaveGame || SaveGame->DataVersion != URSScoreboardSaveGame::CurrentDataVersion || SaveGame->Generation <= 0)
	{
		UE_LOG(LogRSScoreboard, Warning, TEXT("Scoreboard slot %s has unsupported version %d or generation %lld"), *GetSlotName(SlotIndex), SaveGame ? SaveGame->DataVersion : INDEX_NONE, SaveGame ? SaveGame->Generation : 0);
		return false;
	}

	if (SaveGame->Records.Num() > RSScoreboard::MaximumRecordCount)
	{
		UE_LOG(LogRSScoreboard, Warning, TEXT("Scoreboard slot %s contains %d records"), *GetSlotName(SlotIndex), SaveGame->Records.Num());
		return false;
	}

	for (int32 RecordIndex = 0; RecordIndex < SaveGame->Records.Num(); ++RecordIndex)
	{
		const FRSBossClearRecord& Record = SaveGame->Records[RecordIndex];
		FString NormalizedPlayerName;
		if (RSScoreboard::NormalizeAndValidatePlayerName(Record.PlayerName, NormalizedPlayerName) != ERSScoreboardNameValidationResult::Valid
			|| Record.PlayerName != NormalizedPlayerName
			|| Record.RemainingTimeMilliseconds < 0
			|| Record.RemainingTimeMilliseconds % 10 != 0
			|| Record.HitCount < 0)
		{
			UE_LOG(LogRSScoreboard, Warning, TEXT("Scoreboard slot %s contains invalid record %d"), *GetSlotName(SlotIndex), RecordIndex);
			return false;
		}

		if (RecordIndex > 0 && RSScoreboard::IsHigherRanked(Record, SaveGame->Records[RecordIndex - 1]))
		{
			UE_LOG(LogRSScoreboard, Warning, TEXT("Scoreboard slot %s is not sorted at record %d"), *GetSlotName(SlotIndex), RecordIndex);
			return false;
		}

		for (int32 PreviousIndex = 0; PreviousIndex < RecordIndex; ++PreviousIndex)
		{
			if (Record == SaveGame->Records[PreviousIndex])
			{
				UE_LOG(LogRSScoreboard, Warning, TEXT("Scoreboard slot %s contains duplicate record %d"), *GetSlotName(SlotIndex), RecordIndex);
				return false;
			}
		}
	}

	return true;
}

FRSScoreboardSaveOutcome URSScoreboardSubsystem::BuildSaveCandidate(const TArray<FRSBossClearRecord>& CurrentRecords, const FRSBossClearRecord& NewRecord, TArray<FRSBossClearRecord>& OutCandidateRecords)
{
	FRSScoreboardSaveOutcome Outcome;
	OutCandidateRecords.Reset();

	if (CurrentRecords.Contains(NewRecord))
	{
		Outcome.Result = ERSScoreboardSaveResult::Duplicate;
		return Outcome;
	}

	OutCandidateRecords = CurrentRecords;
	OutCandidateRecords.Add(NewRecord);
	RSScoreboard::SortRecords(OutCandidateRecords);

	const int32 NewRecordIndex = OutCandidateRecords.IndexOfByKey(NewRecord);
	if (NewRecordIndex == INDEX_NONE || NewRecordIndex >= RSScoreboard::MaximumRecordCount)
	{
		OutCandidateRecords.Reset();
		Outcome.Result = ERSScoreboardSaveResult::NotRanked;
		return Outcome;
	}

	if (OutCandidateRecords.Num() > RSScoreboard::MaximumRecordCount)
	{
		OutCandidateRecords.SetNum(RSScoreboard::MaximumRecordCount, EAllowShrinking::No);
	}

	Outcome.Result = ERSScoreboardSaveResult::Saved;
	Outcome.DisplayRank = RSScoreboard::GetDisplayRank(OutCandidateRecords, NewRecordIndex, Outcome.bIsTiedRank);
	return Outcome;
}

bool URSScoreboardSubsystem::AreRecordArraysEqual(const TArray<FRSBossClearRecord>& FirstRecords, const TArray<FRSBossClearRecord>& SecondRecords)
{
	if (FirstRecords.Num() != SecondRecords.Num())
	{
		return false;
	}

	for (int32 RecordIndex = 0; RecordIndex < FirstRecords.Num(); ++RecordIndex)
	{
		if (!(FirstRecords[RecordIndex] == SecondRecords[RecordIndex]))
		{
			return false;
		}
	}

	return true;
}

FString URSScoreboardSubsystem::GetSlotName(int32 SlotIndex) const
{
	return FString::Printf(TEXT("%s_%d"), *SlotPrefix, SlotIndex);
}

bool URSScoreboardSubsystem::SaveToSlot(URSScoreboardSaveGame* SaveGame, int32 SlotIndex) const
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bForceSaveFailureForAutomationTest)
	{
		return false;
	}
#endif

	return SaveGame && UGameplayStatics::SaveGameToSlot(SaveGame, GetSlotName(SlotIndex), 0);
}
