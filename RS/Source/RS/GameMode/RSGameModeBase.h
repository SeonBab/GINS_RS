// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "RSBossEncounter.h"
#include "RSBossResultAction.h"
#include "RSInGameMenuAction.h"
#include "RSGameModeBase.generated.h"

class ARSPlayerController;
class USoundBase;

/** 프로젝트의 기본 플레이어 클래스와 인게임 HUD 구성을 지정합니다 */
UCLASS()
class RS_API ARSGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	/** 기본 PlayerState, Pawn, HUD 클래스를 설정합니다 */
	ARSGameModeBase();

protected:
	/** World의 Boss Encounter 결과 이벤트를 연결한 뒤 플레이를 시작합니다 */
	virtual void StartPlay() override;

	/** GameMode가 제거되기 전에 Boss Encounter 결과 이벤트 연결을 해제합니다 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	/** Boss Result Flow가 이미 시작되었는지 반환합니다 */
	bool HasBossResultFlowStarted() const { return BossResult.IsSet(); }

	/** 확정되어 전달된 Boss Encounter 결과를 반환합니다 */
	ERSBossEncounterResult GetBossResult() const { return BossResult.Get(ERSBossEncounterResult::None); }

	/** Local Result UI의 요청을 검증하고 승인된 Level 전환을 한 번만 실행합니다 */
	bool RequestBossResultAction(ARSPlayerController* RequestingPlayerController, ERSBossResultAction Action);

	/** Boss Result Action이 이미 승인되어 처리 중인지 반환합니다 */
	bool IsBossResultActionInProgress() const { return BossResultAction.IsSet(); }

	/** 승인되어 처리 중인 Boss Result Action을 반환합니다 */
	TOptional<ERSBossResultAction> GetBossResultAction() const { return BossResultAction; }

	/** 인게임 메뉴의 Level 전환 요청을 검증하고 한 번만 실행합니다 */
	bool RequestInGameMenuAction(ARSPlayerController* RequestingPlayerController, ERSInGameMenuAction Action);

private:
	/** 현재 World의 초기 음악을 재생 관리자에 요청합니다 */
	void PlayInitialMusic();

	/** 현재 World에 배치된 단일 Boss Encounter를 찾아 결과 이벤트를 연결합니다 */
	void BindBossEncounter();

	/** 실제 연결한 Boss Encounter에서 결과 이벤트를 해제합니다 */
	void UnbindBossEncounter();

	/** 처음 수신한 Boss 결과를 보존하고 Local PlayerController에 한 번만 전달합니다 */
	UFUNCTION()
	void HandleBossEncounterFinished(ARSBossEncounter* BossEncounter, ERSBossEncounterResult Result);

	/** Local PlayerController에 결과와 Encounter 완료 Snapshot을 전달합니다 */
	void RequestLocalBossResultPresentation(ERSBossEncounterResult Result, float RemainingTimeSeconds, int32 HitCount);

	/** 현재 Result Flow와 Action을 검증하고 최초 유효 Action을 커밋합니다 */
	bool TryCommitBossResultAction(ERSBossResultAction Action);

	/** 승인된 Action에 맞는 Level 전환을 실행합니다 */
	void ExecuteBossResultAction(ERSBossResultAction Action, ARSPlayerController* RequestingPlayerController);

	/** 현재 In-game Menu 상태에 유효한 최초 Level 전환 Action을 커밋합니다 */
	bool TryCommitInGameMenuAction(ERSInGameMenuAction Action);

	/** 승인된 인게임 메뉴 Action에 맞는 Level 전환을 실행합니다 */
	void ExecuteInGameMenuAction(ERSInGameMenuAction Action, ARSPlayerController* RequestingPlayerController);

	/** 화면 전환 연출이 끝난 뒤 현재 Level 재시작 또는 Main Menu 이동을 실행합니다 */
	bool ExecuteLevelTransition(bool bRestartCurrentLevel, ARSPlayerController* RequestingPlayerController);

	/** 승인된 전환 대상 Level을 엽니다 */
	void OpenTransitionLevel(FName TargetLevelName);

private:
	friend class FRSBossEncounterStateTest;

	/** 현재 게임 World가 시작될 때 재생할 음악입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Music", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<USoundBase> InitialMusic;

	/** 초기 음악이 재생될 때 적용할 Fade In 시간입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Music", meta = (ClampMin = "0.0", AllowPrivateAccess = "true"))
	float InitialMusicFadeDuration = 1.0f;

	/** 결과 이벤트를 실제로 연결한 Boss Encounter입니다 */
	TWeakObjectPtr<ARSBossEncounter> BoundBossEncounter;

	/** 처음 수신한 Clear 또는 Failed Result이며 중복 Result Flow를 차단합니다 */
	TOptional<ERSBossEncounterResult> BossResult;

	/** Level 전환 호출 전에 커밋하여 중복 Result Action을 차단합니다 */
	TOptional<ERSBossResultAction> BossResultAction;

	/** Level 전환 호출 전에 커밋하여 중복 인게임 메뉴 Action을 차단합니다 */
	TOptional<ERSInGameMenuAction> InGameMenuAction;
};
