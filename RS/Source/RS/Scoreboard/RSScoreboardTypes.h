#pragma once

#include "CoreMinimal.h"
#include "RSScoreboardTypes.generated.h"

/** 플레이어 이름의 정규화와 유효성 검사 결과입니다 */
UENUM(BlueprintType)
enum class ERSScoreboardNameValidationResult : uint8
{
	Valid,
	Empty,
	TooLong,
	InvalidCharacter
};

/** 스코어보드 파일을 읽은 뒤 확정한 상태입니다 */
UENUM(BlueprintType)
enum class ERSScoreboardLoadState : uint8
{
	Empty,
	Loaded,
	RecoveredFromRedundantSlot,
	CorruptOrUnsupported,
	FutureVersion
};

/** 클리어 기록 저장 시도의 결과입니다 */
UENUM(BlueprintType)
enum class ERSScoreboardSaveResult : uint8
{
	Saved,
	InvalidName,
	Duplicate,
	NotRanked,
	LoadUnavailable,
	SaveFailed
};

/** 저장 파일과 화면에 사용하는 보스 클리어 기록입니다 */
USTRUCT(BlueprintType)
struct RS_API FRSBossClearRecord
{
	GENERATED_BODY()

	/** 정규화한 플레이어 표시 이름입니다 */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "RS|Scoreboard")
	FString PlayerName;

	/** 10ms 단위로 반올림한 클리어 순간의 남은 시간입니다 */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "RS|Scoreboard")
	int32 RemainingTimeMilliseconds = 0;

	/** Encounter 진행 중 실제 체력이 감소한 통지 횟수입니다 */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "RS|Scoreboard")
	int32 HitCount = 0;

	bool operator==(const FRSBossClearRecord& Other) const;
};

/** 저장 결과와 저장된 행의 표시 순위입니다 */
USTRUCT(BlueprintType)
struct RS_API FRSScoreboardSaveOutcome
{
	GENERATED_BODY()

	/** 저장 시도의 의미 있는 결과입니다 */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Scoreboard")
	ERSScoreboardSaveResult Result = ERSScoreboardSaveResult::SaveFailed;

	/** 저장 성공 시 경쟁 순위 방식으로 계산한 1-based 순위입니다 */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Scoreboard")
	int32 DisplayRank = 0;

	/** 같은 남은 시간과 피격 횟수를 가진 다른 기록이 있는지입니다 */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Scoreboard")
	bool bIsTiedRank = false;
};

namespace RSScoreboard
{
	constexpr int32 MaximumRecordCount = 100;
	constexpr int32 MaximumPlayerNameLength = 12;

	/** 일반 공백을 정리한 이름과 유효성 결과를 반환합니다 */
	RS_API ERSScoreboardNameValidationResult NormalizeAndValidatePlayerName(const FString& PlayerName, FString& OutNormalizedPlayerName);

	/** 남은 초를 10ms 단위의 음수가 아닌 정수로 변환합니다 */
	RS_API int32 ConvertRemainingTimeToMilliseconds(float RemainingTimeSeconds);

	/** 10ms 단위 남은 시간을 MM:SS.cc 문자열로 변환합니다 */
	RS_API FString FormatRemainingTime(int32 RemainingTimeMilliseconds);

	/** 첫 번째 기록이 두 번째 기록보다 높은 점수인지 반환합니다 */
	RS_API bool IsHigherRanked(const FRSBossClearRecord& FirstRecord, const FRSBossClearRecord& SecondRecord);

	/** 두 기록의 남은 시간과 피격 횟수가 같은지 반환합니다 */
	RS_API bool HasSameScore(const FRSBossClearRecord& FirstRecord, const FRSBossClearRecord& SecondRecord);

	/** 기록을 점수 순서로 안정 정렬합니다 */
	RS_API void SortRecords(TArray<FRSBossClearRecord>& Records);

	/** 정렬된 배열의 행이 표시할 경쟁 순위를 반환합니다 */
	RS_API int32 GetDisplayRank(const TArray<FRSBossClearRecord>& Records, int32 RecordIndex, bool& OutIsTiedRank);
}
