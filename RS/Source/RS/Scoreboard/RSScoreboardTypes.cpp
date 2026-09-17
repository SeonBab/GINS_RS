#include "RSScoreboardTypes.h"

namespace
{
	bool IsAllowedKoreanCharacter(TCHAR Character)
	{
		const uint32 CodePoint = static_cast<uint32>(Character);
		return (CodePoint >= 0x1100 && CodePoint <= 0x11FF)
			|| (CodePoint >= 0x3130 && CodePoint <= 0x318F)
			|| (CodePoint >= 0xA960 && CodePoint <= 0xA97F)
			|| (CodePoint >= 0xAC00 && CodePoint <= 0xD7A3)
			|| (CodePoint >= 0xD7B0 && CodePoint <= 0xD7FF);
	}

	bool IsAllowedPlayerNameCharacter(TCHAR Character)
	{
		return Character == TEXT(' ')
			|| (Character >= TEXT('A') && Character <= TEXT('Z'))
			|| (Character >= TEXT('a') && Character <= TEXT('z'))
			|| (Character >= TEXT('0') && Character <= TEXT('9'))
			|| IsAllowedKoreanCharacter(Character);
	}
}

bool FRSBossClearRecord::operator==(const FRSBossClearRecord& Other) const
{
	return PlayerName == Other.PlayerName
		&& RemainingTimeMilliseconds == Other.RemainingTimeMilliseconds
		&& HitCount == Other.HitCount;
}

ERSScoreboardNameValidationResult RSScoreboard::NormalizeAndValidatePlayerName(const FString& PlayerName, FString& OutNormalizedPlayerName)
{
	OutNormalizedPlayerName.Reset();

	int32 FirstCharacterIndex = 0;
	while (FirstCharacterIndex < PlayerName.Len() && PlayerName[FirstCharacterIndex] == TEXT(' '))
	{
		++FirstCharacterIndex;
	}

	int32 LastCharacterIndex = PlayerName.Len() - 1;
	while (LastCharacterIndex >= FirstCharacterIndex && PlayerName[LastCharacterIndex] == TEXT(' '))
	{
		--LastCharacterIndex;
	}

	bool bPreviousCharacterWasSpace = false;
	for (int32 CharacterIndex = FirstCharacterIndex; CharacterIndex <= LastCharacterIndex; ++CharacterIndex)
	{
		const TCHAR Character = PlayerName[CharacterIndex];
		if (Character == TEXT(' '))
		{
			if (!bPreviousCharacterWasSpace)
			{
				OutNormalizedPlayerName.AppendChar(Character);
			}
			bPreviousCharacterWasSpace = true;
			continue;
		}

		OutNormalizedPlayerName.AppendChar(Character);
		bPreviousCharacterWasSpace = false;
	}

	if (OutNormalizedPlayerName.IsEmpty())
	{
		return ERSScoreboardNameValidationResult::Empty;
	}

	if (OutNormalizedPlayerName.Len() > MaximumPlayerNameLength)
	{
		return ERSScoreboardNameValidationResult::TooLong;
	}

	for (const TCHAR Character : OutNormalizedPlayerName)
	{
		if (!IsAllowedPlayerNameCharacter(Character))
		{
			return ERSScoreboardNameValidationResult::InvalidCharacter;
		}
	}

	return ERSScoreboardNameValidationResult::Valid;
}

int32 RSScoreboard::ConvertRemainingTimeToMilliseconds(float RemainingTimeSeconds)
{
	if (!FMath::IsFinite(RemainingTimeSeconds) || RemainingTimeSeconds <= 0.0f)
	{
		return 0;
	}

	constexpr int64 MaximumCentiseconds = MAX_int32 / 10;
	const int64 RoundedCentiseconds = FMath::RoundToInt64(static_cast<double>(RemainingTimeSeconds) * 100.0);
	return static_cast<int32>(FMath::Clamp<int64>(RoundedCentiseconds, 0, MaximumCentiseconds) * 10);
}

FString RSScoreboard::FormatRemainingTime(int32 RemainingTimeMilliseconds)
{
	const int64 TotalCentiseconds = FMath::Max(0, RemainingTimeMilliseconds) / 10;
	const int64 Minutes = TotalCentiseconds / 6000;
	const int64 Seconds = (TotalCentiseconds / 100) % 60;
	const int64 Centiseconds = TotalCentiseconds % 100;
	return FString::Printf(TEXT("%02lld:%02lld.%02lld"), Minutes, Seconds, Centiseconds);
}

bool RSScoreboard::IsHigherRanked(const FRSBossClearRecord& FirstRecord, const FRSBossClearRecord& SecondRecord)
{
	if (FirstRecord.RemainingTimeMilliseconds != SecondRecord.RemainingTimeMilliseconds)
	{
		return FirstRecord.RemainingTimeMilliseconds > SecondRecord.RemainingTimeMilliseconds;
	}

	return FirstRecord.HitCount < SecondRecord.HitCount;
}

bool RSScoreboard::HasSameScore(const FRSBossClearRecord& FirstRecord, const FRSBossClearRecord& SecondRecord)
{
	return FirstRecord.RemainingTimeMilliseconds == SecondRecord.RemainingTimeMilliseconds
		&& FirstRecord.HitCount == SecondRecord.HitCount;
}

void RSScoreboard::SortRecords(TArray<FRSBossClearRecord>& Records)
{
	Records.StableSort([](const FRSBossClearRecord& FirstRecord, const FRSBossClearRecord& SecondRecord)
	{
		return IsHigherRanked(FirstRecord, SecondRecord);
	});
}

int32 RSScoreboard::GetDisplayRank(const TArray<FRSBossClearRecord>& Records, int32 RecordIndex, bool& OutIsTiedRank)
{
	OutIsTiedRank = false;
	if (!Records.IsValidIndex(RecordIndex))
	{
		return 0;
	}

	int32 FirstTiedRecordIndex = RecordIndex;
	while (FirstTiedRecordIndex > 0 && HasSameScore(Records[FirstTiedRecordIndex - 1], Records[RecordIndex]))
	{
		--FirstTiedRecordIndex;
	}

	OutIsTiedRank = FirstTiedRecordIndex != RecordIndex
		|| (Records.IsValidIndex(RecordIndex + 1) && HasSameScore(Records[RecordIndex], Records[RecordIndex + 1]));
	return FirstTiedRecordIndex + 1;
}
