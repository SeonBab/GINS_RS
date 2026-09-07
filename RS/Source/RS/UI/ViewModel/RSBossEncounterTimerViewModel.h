// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RSLocalPlayerViewModelBase.h"
#include "Tickable.h"
#include "RSBossEncounterTimerViewModel.generated.h"

class ARSBossEncounter;
enum class ERSBossEncounterState : uint8;

/** 현재 보스전의 제한 시간을 사용자 인터페이스에 제공하고 값 변경을 알립니다 */
UCLASS(BlueprintType)
class RS_API URSBossEncounterTimerViewModel : public URSLocalPlayerViewModelBase, public FTickableGameObject
{
	GENERATED_BODY()

#pragma region Tick

public:
	/** 남은 시간이 줄어드는 동안 표시 값을 갱신합니다 */
	virtual void Tick(float DeltaTime) override;

	/** 남은 시간이 실제로 변하는 동안에만 Tick하여 고정된 값을 매 프레임 다시 계산하지 않습니다 */
	virtual bool IsTickable() const override;

	virtual TStatId GetStatId() const override;

	/** 소유한 LocalPlayer의 월드를 따라 Tick하도록 월드를 반환합니다 */
	virtual UWorld* GetTickableGameObjectWorld() const override;

#pragma endregion

public:
	/** 제거되기 전에 Encounter의 이벤트 연결을 해제하고 갱신을 멈춥니다 */
	virtual void BeginDestroy() override;

	/** BossEncounter가 데이터 원본으로 등록되면 제한 시간 표시를 연결합니다 */
	virtual void HandleSourceRegistered(UObject* Source) override;

	/** 현재 BossEncounter가 데이터 원본에서 해제되면 제한 시간 표시를 정리합니다 */
	virtual void HandleSourceUnregistered(UObject* Source) override;

public:
	/** BossEncounter를 연결하고 현재 제한 시간 상태를 동기화합니다 */
	void InitializeViewModel(ARSBossEncounter* InBossEncounter);

	/** Encounter의 이벤트 연결을 해제하고 제한 시간 표시를 초기화합니다 */
	void UninitializeViewModel();

private:
	/**
	 * Encounter의 시작, 종료와 제한 시간 만료를 같은 재평가 경로로 처리합니다
	 * OnEncounterEnded는 Finished와 Active Reset 양쪽에서 발생하므로 이벤트 종류가 아니라 현재 상태로 판단합니다
	 */
	UFUNCTION()
	void HandleEncounterChanged(ARSBossEncounter* InBossEncounter);

	/** 연결된 Encounter의 현재 상태를 표시 값에 동기화합니다 */
	void RefreshFromSource();

	/** 전투 상태와 남은 시간에서 표시 값과 갱신 필요 여부를 함께 결정하며 표시 이력은 보관하지 않습니다 */
	void ApplyEncounterValues(ERSBossEncounterState InEncounterState, bool bInHasTimeLimitExpired, float InRemainingSeconds);

	/** 연결된 데이터 원본이 없을 때 표시 값과 표시 이력을 초기화합니다 */
	void ResetTimerValues();

	/** Encounter의 생명주기를 유지하지 않고 등록한 이벤트만 해제합니다 */
	void DisconnectBossEncounter();

	/** 남은 초를 MM:SS 형식의 표시 문자열로 만듭니다 */
	static FText MakeRemainingTimeText(int32 InTotalSeconds);

private:
	friend class FRSBossEncounterTimerTest;
	friend class FRSBossEncounterStateTest;

	/** 남은 시간이 계속 줄어드는 중인지이며 Tick 활성 조건입니다 */
	bool bIsTimerRunning = false;

	/** 화면에 마지막으로 표시한 정수 초이며 표시 이력이 없으면 -1입니다 */
	int32 DisplayedSeconds = -1;

	/** 현재 제한 시간을 관찰하는 데이터 원본입니다 */
	UPROPERTY(Transient)
	TWeakObjectPtr<ARSBossEncounter> BossEncounter;

	/** 사용자 인터페이스에 제공하는 남은 시간이며 값이 없으면 빈 문자열입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, FieldNotify, Category = "RS|Boss Encounter", meta = (AllowPrivateAccess = "true"))
	FText RemainingTimeText;

	/** 사용자 인터페이스에 제공하는 제한 시간 표시 여부입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, FieldNotify, Category = "RS|Boss Encounter", meta = (AllowPrivateAccess = "true"))
	bool bIsVisible = false;
};
