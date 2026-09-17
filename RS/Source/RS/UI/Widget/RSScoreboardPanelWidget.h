#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RSScoreboardPanelWidget.generated.h"

class UScrollBox;
class UTextBlock;
class URSScoreboardRowWidget;
class URSScoreboardSubsystem;

/** Main Menu와 인게임 Host가 재사용하는 조회 전용 스코어보드 Panel입니다 */
UCLASS(Abstract)
class RS_API URSScoreboardPanelWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	/** 열려 있는 동안의 Subsystem 이벤트 구독을 안전하게 해제합니다 */
	virtual void NativeDestruct() override;

public:
	/** Cache 변경을 구독하고 현재 기록과 Load 상태를 즉시 표시합니다 */
	bool OpenScoreboard();

	/** Cache 변경 구독을 해제합니다 */
	void CloseScoreboard();

private:
	/** 성공 저장으로 Cache가 바뀌면 전체 행을 다시 구성합니다 */
	UFUNCTION()
	void HandleScoreboardChanged();

	/** 현재 Cache와 Load 상태로 행·경고·빈 상태를 갱신합니다 */
	void RefreshScoreboard();

	/** GameInstance 수명의 스코어보드 Subsystem을 반환합니다 */
	URSScoreboardSubsystem* GetScoreboardSubsystem() const;

private:
	/** Cache 기록으로 만든 조회 전용 행을 세로로 Scroll합니다 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "RS|Scoreboard", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UScrollBox> ScrollBox_RecordList;

	/** 일부 복구·손상·미래 버전 Load 상태를 표시합니다 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "RS|Scoreboard", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> TextBlock_LoadStatus;

	/** 정상적으로 읽은 기록이 없을 때 표시합니다 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "RS|Scoreboard", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> TextBlock_EmptyMessage;

	/** 각 Cache 기록을 표시할 조회 전용 행 클래스입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Scoreboard", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<URSScoreboardRowWidget> ScoreboardRowWidgetClass;

	/** 현재 이벤트를 구독한 GameInstance Subsystem입니다 */
	UPROPERTY(Transient)
	TObjectPtr<URSScoreboardSubsystem> BoundScoreboardSubsystem;
};
