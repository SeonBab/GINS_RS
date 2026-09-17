#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RSScoreboardTypes.h"
#include "RSScoreboardRowWidget.generated.h"

class UTextBlock;

/** 스코어보드 기록 하나를 선택 Action 없이 표시합니다 */
UCLASS(Abstract)
class RS_API URSScoreboardRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 경쟁 순위와 클리어 기록을 네 열에 표시합니다 */
	void SetRecord(int32 DisplayRank, const FRSBossClearRecord& Record);

private:
	/** 경쟁 순위 정수입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "RS|Scoreboard", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> TextBlock_Rank;

	/** 기록마다 입력한 플레이어 이름입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "RS|Scoreboard", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> TextBlock_PlayerName;

	/** MM:SS.cc 형식의 클리어 순간 남은 시간입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "RS|Scoreboard", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> TextBlock_RemainingTime;

	/** 실제 체력 감소 통지 횟수입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "RS|Scoreboard", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> TextBlock_HitCount;
};
