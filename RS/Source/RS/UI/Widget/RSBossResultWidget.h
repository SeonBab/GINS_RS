// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RSBossResultAction.h"
#include "RSScoreboardTypes.h"
#include "RSViewModelWidget.h"
#include "RSBossResultWidget.generated.h"

class UButton;
class UEditableTextBox;
class UTextBlock;
class UWidget;
enum class ERSBossEncounterResult : uint8;

DECLARE_MULTICAST_DELEGATE_OneParam(FRSBossResultActionRequested, ERSBossResultAction)

/** 확정된 보스전 결과의 제목과 안내 문구를 표시하는 공통 Widget 기반입니다 */
UCLASS()
class RS_API URSBossResultWidget : public URSViewModelWidget
{
	GENERATED_BODY()

protected:
	/** 저장 에셋에 저작한 두 Action Button의 클릭 이벤트를 연결합니다 */
	virtual void NativeOnInitialized() override;

public:
	/** Result Action 요청 이벤트를 반환합니다 */
	FRSBossResultActionRequested& GetBossResultActionRequested() { return OnBossResultActionRequested; }

	/** 두 Result Action Button의 입력 가능 상태를 함께 변경합니다 */
	void SetActionsEnabled(bool bEnabled);

	/** 확정된 결과와 완료 Snapshot으로 이번 Result Cycle의 기록 저장 상태를 초기화합니다 */
	void PresentResult(ERSBossEncounterResult Result, float RemainingTimeSeconds, int32 HitCount);

private:
	/** 현재 입력을 공용 규칙으로 검사하고 저장 Action과 안내 문구를 갱신합니다 */
	void RefreshNameValidation(const FText& PlayerName);

	/** 이름 검증 결과를 사용자 안내 문구로 변환합니다 */
	static FText GetNameValidationMessage(ERSScoreboardNameValidationResult ValidationResult);

	/** Load 차단 상태를 사용자 안내 문구로 변환합니다 */
	static FText GetLoadUnavailableMessage(ERSScoreboardLoadState LoadState);

	/** 저장 결과를 사용자 안내 문구로 변환합니다 */
	static FText GetSaveOutcomeMessage(const FRSScoreboardSaveOutcome& SaveOutcome);

	/** 이름 입력 변경을 실시간 검증에 반영합니다 */
	UFUNCTION()
	void HandlePlayerNameTextChanged(const FText& PlayerName);

	/** 현재 Clear Snapshot과 입력 이름으로 기록 저장을 시도합니다 */
	UFUNCTION()
	void HandleSaveRecordButtonClicked();

	/** 현재 Level 재시작 Action을 요청합니다 */
	UFUNCTION()
	void HandleRestartButtonClicked();

	/** Main Menu Level 이동 Action을 요청합니다 */
	UFUNCTION()
	void HandleMainMenuButtonClicked();

private:
	/** 현재 Level 재시작을 요청하는 Button입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "RS|Boss Result", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> Button_Restart;

	/** Main Menu Level 이동을 요청하는 Button입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "RS|Boss Result", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> Button_MainMenu;

	/** Clear Result에서 남은 시간을 표시합니다 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "RS|Boss Result", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> TextBlock_RemainingTime;

	/** Clear Result에서 피격 횟수를 표시합니다 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "RS|Boss Result", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> TextBlock_HitCount;

	/** Clear 기록에 사용할 플레이어 이름 입력란입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "RS|Boss Result", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UEditableTextBox> EditableTextBox_PlayerName;

	/** 이번 Clear 기록의 선택 저장을 요청하는 Button입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "RS|Boss Result", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> Button_SaveRecord;

	/** 이름 검증·Load 상태·저장 결과 안내를 표시합니다 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "RS|Boss Result", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> TextBlock_RecordFeedback;

	/** Failed Result에서 Clear 전용 통계와 저장 UI를 함께 숨기는 컨테이너입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "RS|Boss Result", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> Panel_ClearRecord;

	/** Widget의 Action 의도를 HUD에 전달하는 출력 이벤트입니다 */
	FRSBossResultActionRequested OnBossResultActionRequested;

	/** 이번 Result Cycle의 완료 순간 남은 시간입니다 */
	float ResultRemainingTimeSeconds = 0.0f;

	/** 이번 Result Cycle의 완료 순간 피격 횟수입니다 */
	int32 ResultHitCount = 0;

	/** 이번 Result가 기록 저장 대상인 Clear인지입니다 */
	bool bIsClearResult = false;

	/** 사용자가 이번 Result에서 이름 입력을 시작했는지입니다 */
	bool bHasEditedPlayerName = false;

	/** Saved 또는 NotRanked로 이번 Result Cycle의 저장 입력이 끝났는지입니다 */
	bool bIsRecordInputFinalized = false;

	/** 정규화한 이름을 입력란에 반영할 때 TextChanged 재진입을 구분합니다 */
	bool bIsUpdatingPlayerName = false;
};
