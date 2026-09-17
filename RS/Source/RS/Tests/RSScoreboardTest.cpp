#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "RSScoreboardSaveGame.h"
#include "RSScoreboardSubsystem.h"
#include "RSScoreboardTestTypes.h"
#include "RSScoreboardTypes.h"

namespace
{
	constexpr int32 ScoreboardSlotCount = 3;

	FRSBossClearRecord MakeRecord(const FString& PlayerName, int32 RemainingTimeMilliseconds, int32 HitCount)
	{
		FRSBossClearRecord Record;
		Record.PlayerName = PlayerName;
		Record.RemainingTimeMilliseconds = RemainingTimeMilliseconds;
		Record.HitCount = HitCount;
		return Record;
	}

	class FScopedScoreboardTestSlots
	{
	public:
		FScopedScoreboardTestSlots()
			: SlotPrefix(FString::Printf(TEXT("RSScoreboardAutomation_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits)))
		{
			DeleteSlots();
		}

		~FScopedScoreboardTestSlots()
		{
			DeleteSlots();
		}

		const FString& GetSlotPrefix() const { return SlotPrefix; }

		FString GetSlotName(int32 SlotIndex) const
		{
			return FString::Printf(TEXT("%s_%d"), *SlotPrefix, SlotIndex);
		}

		void DeleteSlots() const
		{
			for (int32 SlotIndex = 0; SlotIndex < ScoreboardSlotCount; ++SlotIndex)
			{
				UGameplayStatics::DeleteGameInSlot(GetSlotName(SlotIndex), 0);
			}
		}

	private:
		FString SlotPrefix;
	};

	URSScoreboardSubsystem* CreateTestSubsystem(const FString& SlotPrefix)
	{
		UGameInstance* GameInstance = NewObject<UGameInstance>();
		URSScoreboardSubsystem* ScoreboardSubsystem = NewObject<URSScoreboardSubsystem>(GameInstance);
		ScoreboardSubsystem->InitializeForAutomationTest(SlotPrefix);
		return ScoreboardSubsystem;
	}

	bool WriteSaveGameSlot(const FString& SlotName, int32 DataVersion, int64 Generation, const TArray<FRSBossClearRecord>& Records)
	{
		URSScoreboardSaveGame* SaveGame = NewObject<URSScoreboardSaveGame>();
		SaveGame->DataVersion = DataVersion;
		SaveGame->Generation = Generation;
		SaveGame->Records = Records;
		return UGameplayStatics::SaveGameToSlot(SaveGame, SlotName, 0);
	}

	URSScoreboardSaveGame* ReadSaveGameSlot(const FString& SlotName)
	{
		return Cast<URSScoreboardSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSScoreboardNameAndTimeTest, "RS.Scoreboard.NameAndTime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSScoreboardNameAndTimeTest::RunTest(const FString& Parameters)
{
	FString NormalizedPlayerName;
	TestEqual(TEXT("Ordinary spaces are normalized"), RSScoreboard::NormalizeAndValidatePlayerName(TEXT("  GINS   PLAYER  "), NormalizedPlayerName), ERSScoreboardNameValidationResult::Valid);
	TestEqual(TEXT("Normalized name is returned"), NormalizedPlayerName, FString(TEXT("GINS PLAYER")));

	TestEqual(TEXT("Hangul, ASCII letters, digits, and word spaces are valid"), RSScoreboard::NormalizeAndValidatePlayerName(TEXT("플레이어 01"), NormalizedPlayerName), ERSScoreboardNameValidationResult::Valid);
	TestEqual(TEXT("Twelve characters are valid"), RSScoreboard::NormalizeAndValidatePlayerName(TEXT("ABCDEFGHIJKL"), NormalizedPlayerName), ERSScoreboardNameValidationResult::Valid);
	TestEqual(TEXT("Thirteen characters are too long"), RSScoreboard::NormalizeAndValidatePlayerName(TEXT("ABCDEFGHIJKLM"), NormalizedPlayerName), ERSScoreboardNameValidationResult::TooLong);
	TestEqual(TEXT("Only ordinary spaces are empty"), RSScoreboard::NormalizeAndValidatePlayerName(TEXT("     "), NormalizedPlayerName), ERSScoreboardNameValidationResult::Empty);
	TestEqual(TEXT("Tab is not trimmed or accepted"), RSScoreboard::NormalizeAndValidatePlayerName(TEXT("Player\t"), NormalizedPlayerName), ERSScoreboardNameValidationResult::InvalidCharacter);
	TestEqual(TEXT("Special character is rejected"), RSScoreboard::NormalizeAndValidatePlayerName(TEXT("Player!"), NormalizedPlayerName), ERSScoreboardNameValidationResult::InvalidCharacter);
	TestEqual(TEXT("Emoji surrogate pair is rejected"), RSScoreboard::NormalizeAndValidatePlayerName(TEXT("Player😀"), NormalizedPlayerName), ERSScoreboardNameValidationResult::InvalidCharacter);

	TestEqual(TEXT("Negative time clamps to zero"), RSScoreboard::ConvertRemainingTimeToMilliseconds(-1.0f), 0);
	TestEqual(TEXT("Time rounds down to a centisecond"), RSScoreboard::ConvertRemainingTimeToMilliseconds(12.344f), 12340);
	TestEqual(TEXT("Time rounds up to a centisecond"), RSScoreboard::ConvertRemainingTimeToMilliseconds(12.346f), 12350);
	TestEqual(TEXT("Zero time format"), RSScoreboard::FormatRemainingTime(0), FString(TEXT("00:00.00")));
	TestEqual(TEXT("Minute time format"), RSScoreboard::FormatRemainingTime(61010), FString(TEXT("01:01.01")));
	TestEqual(TEXT("Minutes can exceed two digits"), RSScoreboard::FormatRemainingTime(6000010), FString(TEXT("100:00.01")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSScoreboardRankingTest, "RS.Scoreboard.Ranking", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSScoreboardRankingTest::RunTest(const FString& Parameters)
{
	TArray<FRSBossClearRecord> Records =
	{
		MakeRecord(TEXT("First"), 60000, 2),
		MakeRecord(TEXT("Second"), 60000, 2),
		MakeRecord(TEXT("Lower"), 50000, 0),
		MakeRecord(TEXT("HitMore"), 60000, 3),
		MakeRecord(TEXT("Higher"), 70000, 9)
	};
	RSScoreboard::SortRecords(Records);

	TestEqual(TEXT("Higher remaining time sorts first"), Records[0].PlayerName, FString(TEXT("Higher")));
	TestEqual(TEXT("First tied record keeps stable order"), Records[1].PlayerName, FString(TEXT("First")));
	TestEqual(TEXT("Second tied record keeps stable order"), Records[2].PlayerName, FString(TEXT("Second")));
	TestEqual(TEXT("Fewer hits wins after equal time"), Records[3].PlayerName, FString(TEXT("HitMore")));

	bool bIsTiedRank = false;
	TestEqual(TEXT("First tied record uses competition rank"), RSScoreboard::GetDisplayRank(Records, 1, bIsTiedRank), 2);
	TestTrue(TEXT("First tied record reports a tie"), bIsTiedRank);
	TestEqual(TEXT("Second tied record shares competition rank"), RSScoreboard::GetDisplayRank(Records, 2, bIsTiedRank), 2);
	TestTrue(TEXT("Second tied record reports a tie"), bIsTiedRank);
	TestEqual(TEXT("Following record skips tied positions"), RSScoreboard::GetDisplayRank(Records, 3, bIsTiedRank), 4);
	TestFalse(TEXT("Following record is not tied"), bIsTiedRank);

	TArray<FRSBossClearRecord> CandidateRecords;
	const FRSScoreboardSaveOutcome DuplicateOutcome = URSScoreboardSubsystem::BuildSaveCandidateForAutomationTest(Records, Records[1], CandidateRecords);
	TestEqual(TEXT("Exact duplicate is rejected"), DuplicateOutcome.Result, ERSScoreboardSaveResult::Duplicate);
	TestTrue(TEXT("Duplicate does not produce a candidate array"), CandidateRecords.IsEmpty());

	const FRSBossClearRecord NewTiedRecord = MakeRecord(TEXT("Third"), 60000, 2);
	const FRSScoreboardSaveOutcome TiedOutcome = URSScoreboardSubsystem::BuildSaveCandidateForAutomationTest(Records, NewTiedRecord, CandidateRecords);
	TestEqual(TEXT("Different name with same score is saved"), TiedOutcome.Result, ERSScoreboardSaveResult::Saved);
	TestEqual(TEXT("New tied record shares the display rank"), TiedOutcome.DisplayRank, 2);
	TestTrue(TEXT("New tied record reports a tied rank"), TiedOutcome.bIsTiedRank);

	TArray<FRSBossClearRecord> FullRecords;
	for (int32 RecordIndex = 0; RecordIndex < RSScoreboard::MaximumRecordCount; ++RecordIndex)
	{
		FullRecords.Add(MakeRecord(FString::Printf(TEXT("P%d"), RecordIndex), 100000 - RecordIndex * 10, 0));
	}

	const FRSBossClearRecord BoundaryTieRecord = MakeRecord(TEXT("Boundary"), FullRecords.Last().RemainingTimeMilliseconds, 0);
	const FRSScoreboardSaveOutcome BoundaryOutcome = URSScoreboardSubsystem::BuildSaveCandidateForAutomationTest(FullRecords, BoundaryTieRecord, CandidateRecords);
	TestEqual(TEXT("New tie after the final stable row is not ranked"), BoundaryOutcome.Result, ERSScoreboardSaveResult::NotRanked);
	TestTrue(TEXT("Not ranked result does not expose a write candidate"), CandidateRecords.IsEmpty());

	const FRSBossClearRecord BetterRecord = MakeRecord(TEXT("Better"), FullRecords.Last().RemainingTimeMilliseconds + 10, 0);
	const FRSScoreboardSaveOutcome BetterOutcome = URSScoreboardSubsystem::BuildSaveCandidateForAutomationTest(FullRecords, BetterRecord, CandidateRecords);
	TestEqual(TEXT("Better boundary record is saved"), BetterOutcome.Result, ERSScoreboardSaveResult::Saved);
	TestEqual(TEXT("Saved candidate remains strictly capped"), CandidateRecords.Num(), RSScoreboard::MaximumRecordCount);
	TestFalse(TEXT("Displaced lowest record is removed"), CandidateRecords.Contains(FullRecords.Last()));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSScoreboardStorageTest, "RS.Scoreboard.Storage", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSScoreboardStorageTest::RunTest(const FString& Parameters)
{
	FScopedScoreboardTestSlots TestSlots;
	URSScoreboardSubsystem* ScoreboardSubsystem = CreateTestSubsystem(TestSlots.GetSlotPrefix());
	URSScoreboardTestListener* TestListener = NewObject<URSScoreboardTestListener>();
	TestListener->ListenTo(ScoreboardSubsystem);
	TestEqual(TEXT("Missing slots start empty"), ScoreboardSubsystem->GetLoadState(), ERSScoreboardLoadState::Empty);
	TestTrue(TEXT("Empty state accepts saves"), ScoreboardSubsystem->CanSaveRecords());

	TestEqual(TEXT("First save succeeds"), ScoreboardSubsystem->TrySaveClearRecord(TEXT("Alpha"), 30.0f, 3).Result, ERSScoreboardSaveResult::Saved);
	TestEqual(TEXT("First generation is committed"), ScoreboardSubsystem->GetGenerationForAutomationTest(), static_cast<int64>(1));
	TestEqual(TEXT("First generation uses slot zero"), ScoreboardSubsystem->GetActiveSlotIndexForAutomationTest(), 0);
	TestTrue(TEXT("Slot zero exists after first save"), UGameplayStatics::DoesSaveGameExist(TestSlots.GetSlotName(0), 0));
	TestEqual(TEXT("Successful save broadcasts one cache change"), TestListener->ChangedCount, 1);

	ScoreboardSubsystem->SetForceSaveFailureForAutomationTest(true);
	AddExpectedError(TEXT("Failed to save scoreboard slot"), EAutomationExpectedErrorFlags::Contains, 1);
	TestEqual(TEXT("Forced file failure is reported"), ScoreboardSubsystem->TrySaveClearRecord(TEXT("Bravo"), 40.0f, 2).Result, ERSScoreboardSaveResult::SaveFailed);
	TestEqual(TEXT("Failed save keeps generation"), ScoreboardSubsystem->GetGenerationForAutomationTest(), static_cast<int64>(1));
	TestEqual(TEXT("Failed save keeps cache"), ScoreboardSubsystem->GetRecords().Num(), 1);
	TestFalse(TEXT("Failed save does not create next slot"), UGameplayStatics::DoesSaveGameExist(TestSlots.GetSlotName(1), 0));
	TestEqual(TEXT("Failed save does not broadcast a cache change"), TestListener->ChangedCount, 1);

	ScoreboardSubsystem->SetForceSaveFailureForAutomationTest(false);
	TestEqual(TEXT("Retry saves the same next generation"), ScoreboardSubsystem->TrySaveClearRecord(TEXT("Bravo"), 40.0f, 2).Result, ERSScoreboardSaveResult::Saved);
	TestEqual(TEXT("Second generation is committed"), ScoreboardSubsystem->GetGenerationForAutomationTest(), static_cast<int64>(2));
	TestEqual(TEXT("Third generation saves"), ScoreboardSubsystem->TrySaveClearRecord(TEXT("Charlie"), 50.0f, 1).Result, ERSScoreboardSaveResult::Saved);
	TestEqual(TEXT("Fourth generation rotates to slot zero"), ScoreboardSubsystem->TrySaveClearRecord(TEXT("Delta"), 60.0f, 0).Result, ERSScoreboardSaveResult::Saved);
	TestEqual(TEXT("Each successful save broadcasts once"), TestListener->ChangedCount, 4);

	URSScoreboardSaveGame* SlotZeroSaveGame = ReadSaveGameSlot(TestSlots.GetSlotName(0));
	URSScoreboardSaveGame* SlotOneSaveGame = ReadSaveGameSlot(TestSlots.GetSlotName(1));
	URSScoreboardSaveGame* SlotTwoSaveGame = ReadSaveGameSlot(TestSlots.GetSlotName(2));
	TestNotNull(TEXT("Rotated slot zero can be loaded"), SlotZeroSaveGame);
	TestNotNull(TEXT("Slot one can be loaded"), SlotOneSaveGame);
	TestNotNull(TEXT("Slot two can be loaded"), SlotTwoSaveGame);
	if (SlotZeroSaveGame && SlotOneSaveGame && SlotTwoSaveGame)
	{
		TestEqual(TEXT("Slot zero has fourth generation"), SlotZeroSaveGame->Generation, static_cast<int64>(4));
		TestEqual(TEXT("Slot one has second generation"), SlotOneSaveGame->Generation, static_cast<int64>(2));
		TestEqual(TEXT("Slot two has third generation"), SlotTwoSaveGame->Generation, static_cast<int64>(3));
	}

	URSScoreboardSubsystem* ReloadedSubsystem = CreateTestSubsystem(TestSlots.GetSlotPrefix());
	TestEqual(TEXT("Latest valid generation loads"), ReloadedSubsystem->GetGenerationForAutomationTest(), static_cast<int64>(4));
	TestEqual(TEXT("Latest generation restores all records"), ReloadedSubsystem->GetRecords().Num(), 4);
	TestEqual(TEXT("Complete valid slots load normally"), ReloadedSubsystem->GetLoadState(), ERSScoreboardLoadState::Loaded);

	TestTrue(TEXT("Invalid older slot can be written for recovery test"), WriteSaveGameSlot(TestSlots.GetSlotName(1), 0, 5, {}));
	AddExpectedError(TEXT("has unsupported version 0 or generation 5"), EAutomationExpectedErrorFlags::Contains, 1);
	ReloadedSubsystem->ReloadForAutomationTest();
	TestEqual(TEXT("Invalid slot recovers from latest valid generation"), ReloadedSubsystem->GetLoadState(), ERSScoreboardLoadState::RecoveredFromRedundantSlot);
	TestEqual(TEXT("Recovery keeps latest valid generation"), ReloadedSubsystem->GetGenerationForAutomationTest(), static_cast<int64>(4));
	TestTrue(TEXT("Recovered state accepts a repair save"), ReloadedSubsystem->CanSaveRecords());
	TestEqual(TEXT("Next save naturally replaces invalid target slot"), ReloadedSubsystem->TrySaveClearRecord(TEXT("Echo"), 70.0f, 0).Result, ERSScoreboardSaveResult::Saved);
	TestEqual(TEXT("Replacing the only invalid slot returns to loaded"), ReloadedSubsystem->GetLoadState(), ERSScoreboardLoadState::Loaded);

	TestTrue(TEXT("Future version slot can be written"), WriteSaveGameSlot(TestSlots.GetSlotName(2), URSScoreboardSaveGame::CurrentDataVersion + 1, 6, {}));
	AddExpectedError(TEXT("uses future data version 2"), EAutomationExpectedErrorFlags::Contains, 1);
	ReloadedSubsystem->ReloadForAutomationTest();
	TestEqual(TEXT("Any readable future version blocks the cache"), ReloadedSubsystem->GetLoadState(), ERSScoreboardLoadState::FutureVersion);
	TestFalse(TEXT("Future version blocks saves"), ReloadedSubsystem->CanSaveRecords());
	TestTrue(TEXT("Future version hides older cache"), ReloadedSubsystem->GetRecords().IsEmpty());
	TestEqual(TEXT("Save attempt reports unavailable load"), ReloadedSubsystem->TrySaveClearRecord(TEXT("Foxtrot"), 80.0f, 0).Result, ERSScoreboardSaveResult::LoadUnavailable);

	TestSlots.DeleteSlots();
	const TArray<FRSBossClearRecord> FirstConflictRecords = { MakeRecord(TEXT("Alpha"), 1000, 0) };
	const TArray<FRSBossClearRecord> SecondConflictRecords = { MakeRecord(TEXT("Bravo"), 1000, 0) };
	TestTrue(TEXT("First conflicting generation can be written"), WriteSaveGameSlot(TestSlots.GetSlotName(0), URSScoreboardSaveGame::CurrentDataVersion, 7, FirstConflictRecords));
	TestTrue(TEXT("Second conflicting generation can be written"), WriteSaveGameSlot(TestSlots.GetSlotName(1), URSScoreboardSaveGame::CurrentDataVersion, 7, SecondConflictRecords));
	AddExpectedError(TEXT("disagree at generation 7"), EAutomationExpectedErrorFlags::Contains, 1);
	ReloadedSubsystem->ReloadForAutomationTest();
	TestEqual(TEXT("Different data at one generation is ambiguous"), ReloadedSubsystem->GetLoadState(), ERSScoreboardLoadState::CorruptOrUnsupported);
	TestFalse(TEXT("Ambiguous generation blocks saves"), ReloadedSubsystem->CanSaveRecords());

	TestSlots.DeleteSlots();
	TestTrue(TEXT("First identical generation can be written"), WriteSaveGameSlot(TestSlots.GetSlotName(0), URSScoreboardSaveGame::CurrentDataVersion, 8, FirstConflictRecords));
	TestTrue(TEXT("Second identical generation can be written"), WriteSaveGameSlot(TestSlots.GetSlotName(1), URSScoreboardSaveGame::CurrentDataVersion, 8, FirstConflictRecords));
	ReloadedSubsystem->ReloadForAutomationTest();
	TestEqual(TEXT("Identical data at one generation is valid"), ReloadedSubsystem->GetLoadState(), ERSScoreboardLoadState::Loaded);
	TestEqual(TEXT("Identical generation chooses lower slot deterministically"), ReloadedSubsystem->GetActiveSlotIndexForAutomationTest(), 0);

	TestSlots.DeleteSlots();
	TestTrue(TEXT("Unsupported old version can be written"), WriteSaveGameSlot(TestSlots.GetSlotName(0), 0, 1, {}));
	AddExpectedError(TEXT("has unsupported version 0 or generation 1"), EAutomationExpectedErrorFlags::Contains, 1);
	ReloadedSubsystem->ReloadForAutomationTest();
	TestEqual(TEXT("Only unsupported slots block load"), ReloadedSubsystem->GetLoadState(), ERSScoreboardLoadState::CorruptOrUnsupported);
	TestFalse(TEXT("Unsupported slots block saves"), ReloadedSubsystem->CanSaveRecords());

	TestSlots.DeleteSlots();
	ReloadedSubsystem->ReloadForAutomationTest();
	TestEqual(TEXT("Removing only test slots returns to empty"), ReloadedSubsystem->GetLoadState(), ERSScoreboardLoadState::Empty);
	TestListener->StopListening();

	return true;
}

#endif
