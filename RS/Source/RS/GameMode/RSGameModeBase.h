// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "RSBossEncounter.h"
#include "RSBossResultAction.h"
#include "RSGameModeBase.generated.h"

class ARSPlayerController;

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

private:
	/** 현재 World에 배치된 단일 Boss Encounter를 찾아 결과 이벤트를 연결합니다 */
	void BindBossEncounter();

	/** 실제 연결한 Boss Encounter에서 결과 이벤트를 해제합니다 */
	void UnbindBossEncounter();

	/** 처음 수신한 Boss 결과를 보존하고 Local PlayerController에 한 번만 전달합니다 */
	UFUNCTION()
	void HandleBossEncounterFinished(ARSBossEncounter* BossEncounter, ERSBossEncounterResult Result);

	/** Local PlayerController의 중립적인 Result Presentation 경계에 결과를 전달합니다 */
	void RequestLocalBossResultPresentation(ERSBossEncounterResult Result);

	/** 현재 Result Flow와 Action을 검증하고 최초 유효 Action을 커밋합니다 */
	bool TryCommitBossResultAction(ERSBossResultAction Action);

	/** 승인된 Action에 맞는 Level 전환을 실행합니다 */
	void ExecuteBossResultAction(ERSBossResultAction Action);

private:
	friend class FRSBossEncounterStateTest;

	/** 결과 이벤트를 실제로 연결한 Boss Encounter입니다 */
	TWeakObjectPtr<ARSBossEncounter> BoundBossEncounter;

	/** 처음 수신한 Clear 또는 Failed Result이며 중복 Result Flow를 차단합니다 */
	TOptional<ERSBossEncounterResult> BossResult;

	/** Level 전환 호출 전에 커밋하여 중복 Result Action을 차단합니다 */
	TOptional<ERSBossResultAction> BossResultAction;
};
