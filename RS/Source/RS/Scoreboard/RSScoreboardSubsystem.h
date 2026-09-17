#pragma once

#include "CoreMinimal.h"
#include "RSScoreboardTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RSScoreboardSubsystem.generated.h"

class URSScoreboardSaveGame;

/** 확정 Cache가 파일 저장 성공으로 바뀐 뒤 발생합니다 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRSScoreboardChangedSignature);

/** 세 SaveGame Slot의 검증·정렬·복구와 확정 기록 Cache를 소유합니다 */
UCLASS()
class RS_API URSScoreboardSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** 프로덕션 세 Slot을 한 번 읽어 GameInstance 수명의 Cache를 구성합니다 */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** 변경 알림과 Runtime Cache를 정리합니다 */
	virtual void Deinitialize() override;

public:
	/** 현재 확정된 정렬 기록의 복사본을 반환합니다 */
	UFUNCTION(BlueprintPure, Category = "RS|Scoreboard")
	TArray<FRSBossClearRecord> GetRecords() const { return Records; }

	/** 세 Slot을 읽어 확정한 현재 상태를 반환합니다 */
	UFUNCTION(BlueprintPure, Category = "RS|Scoreboard")
	ERSScoreboardLoadState GetLoadState() const { return LoadState; }

	/** 기존 파일을 훼손하지 않고 새 기록을 저장할 수 있는지 반환합니다 */
	UFUNCTION(BlueprintPure, Category = "RS|Scoreboard")
	bool CanSaveRecords() const;

	/** UI 사전 검사에 사용할 정규화 이름과 유효성 결과를 반환합니다 */
	UFUNCTION(BlueprintPure, Category = "RS|Scoreboard")
	ERSScoreboardNameValidationResult NormalizeAndValidatePlayerName(const FString& PlayerName, FString& NormalizedPlayerName) const;

	/** 남은 초를 저장·정렬에 사용하는 10ms 단위 정수로 변환합니다 */
	UFUNCTION(BlueprintPure, Category = "RS|Scoreboard")
	int32 ConvertRemainingTimeToMilliseconds(float RemainingTimeSeconds) const;

	/** 저장된 남은 시간을 MM:SS.cc 형식으로 변환합니다 */
	UFUNCTION(BlueprintPure, Category = "RS|Scoreboard")
	FString FormatRemainingTime(int32 RemainingTimeMilliseconds) const;

	/** 이름과 완료 Snapshot으로 새 기록 저장을 시도합니다 */
	UFUNCTION(BlueprintCallable, Category = "RS|Scoreboard")
	FRSScoreboardSaveOutcome TrySaveClearRecord(const FString& PlayerName, float RemainingTimeSeconds, int32 HitCount);

public:
	/** 파일 성공 뒤 확정 Cache가 바뀌었음을 UI에 전달합니다 */
	UPROPERTY(BlueprintAssignable, Category = "RS|Scoreboard")
	FRSScoreboardChangedSignature OnScoreboardChanged;

#if WITH_DEV_AUTOMATION_TESTS
public:
	/** 자동 테스트 전용 Slot 접두사로 상태를 다시 구성합니다 */
	void InitializeForAutomationTest(const FString& InSlotPrefix);

	/** 현재 테스트 Slot의 파일 상태를 다시 읽습니다 */
	void ReloadForAutomationTest();

	/** 다음 저장을 파일 시스템과 무관하게 실패시킵니다 */
	void SetForceSaveFailureForAutomationTest(bool bInForceSaveFailure) { bForceSaveFailureForAutomationTest = bInForceSaveFailure; }

	/** 현재 확정한 세대를 반환합니다 */
	int64 GetGenerationForAutomationTest() const { return CurrentGeneration; }

	/** 현재 선택한 Slot Index를 반환합니다 */
	int32 GetActiveSlotIndexForAutomationTest() const { return ActiveSlotIndex; }

	/** 지정한 Index의 현재 테스트 Slot 이름을 반환합니다 */
	FString GetSlotNameForAutomationTest(int32 SlotIndex) const;

	/** 파일 쓰기 전 후보 판정 규칙만 검증합니다 */
	static FRSScoreboardSaveOutcome BuildSaveCandidateForAutomationTest(const TArray<FRSBossClearRecord>& CurrentRecords, const FRSBossClearRecord& NewRecord, TArray<FRSBossClearRecord>& OutCandidateRecords);
#endif

private:
	/** 세 Slot을 검증하고 최신 유효 세대로 Cache를 교체합니다 */
	void LoadScoreboard();

	/** 지정한 SaveGame이 현재 버전의 모든 불변식을 만족하는지 확인합니다 */
	bool IsSaveGameValid(const URSScoreboardSaveGame* SaveGame, int32 SlotIndex) const;

	/** 저장 후보를 정렬하고 중복·상위 100개·표시 순위를 판정합니다 */
	static FRSScoreboardSaveOutcome BuildSaveCandidate(const TArray<FRSBossClearRecord>& CurrentRecords, const FRSBossClearRecord& NewRecord, TArray<FRSBossClearRecord>& OutCandidateRecords);

	/** 동일 Generation의 두 유효 파일이 같은 기록을 담는지 확인합니다 */
	static bool AreRecordArraysEqual(const TArray<FRSBossClearRecord>& FirstRecords, const TArray<FRSBossClearRecord>& SecondRecords);

	/** 현재 접두사와 Index로 실제 Slot 이름을 만듭니다 */
	FString GetSlotName(int32 SlotIndex) const;

	/** SaveGame을 지정한 Slot 하나에 기록합니다 */
	bool SaveToSlot(URSScoreboardSaveGame* SaveGame, int32 SlotIndex) const;

private:
	/** GameInstance 수명에서 사용하는 확정 기록 Cache입니다 */
	TArray<FRSBossClearRecord> Records;

	/** 기존 파일을 보호하기 위한 현재 Load 상태입니다 */
	ERSScoreboardLoadState LoadState = ERSScoreboardLoadState::Empty;

	/** Cache가 선택된 파일의 세대이며 빈 상태에서는 0입니다 */
	int64 CurrentGeneration = 0;

	/** Cache가 선택된 Slot Index이며 빈 상태에서는 INDEX_NONE입니다 */
	int32 ActiveSlotIndex = INDEX_NONE;

	/** 존재하지만 검증에 실패해 순환 저장으로 복구해야 하는 Slot입니다 */
	TSet<int32> InvalidSlotIndices;

	/** 프로덕션 또는 자동 테스트의 세 Slot이 공유하는 접두사입니다 */
	FString SlotPrefix = TEXT("BossClearScoreboard");

#if WITH_DEV_AUTOMATION_TESTS
	/** 자동 테스트에서 Cache 불변식을 실제 디스크 오류와 무관하게 확인합니다 */
	bool bForceSaveFailureForAutomationTest = false;
#endif
};
