#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "RSScoreboardTypes.h"
#include "RSScoreboardSaveGame.generated.h"

/** 한 세대의 정렬된 스코어보드 기록을 직렬화합니다 */
UCLASS()
class RS_API URSScoreboardSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr int32 CurrentDataVersion = 1;

public:
	/** 저장 구조의 명시적 호환성 버전입니다 */
	UPROPERTY(SaveGame)
	int32 DataVersion = CurrentDataVersion;

	/** 세 Slot 가운데 가장 최신의 유효한 데이터를 선택하는 단조 증가 세대입니다 */
	UPROPERTY(SaveGame)
	int64 Generation = 0;

	/** 점수 순서로 정렬된 최대 50개의 클리어 기록입니다 */
	UPROPERTY(SaveGame)
	TArray<FRSBossClearRecord> Records;
};
