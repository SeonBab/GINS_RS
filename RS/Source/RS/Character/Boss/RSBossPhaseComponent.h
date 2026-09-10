// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RSBossPhaseComponent.generated.h"

class APawn;
class URSBaseGameplayAbility;
class URSBossPhaseData;
class URSHealthComponent;

struct FRSBossPhaseDefinition;

/** 보스가 사용하는 패턴의 분류입니다 */
UENUM(BlueprintType)
enum class ERSBossPatternType : uint8
{
	/** 사이클에서 기본으로 반복하는 상시 패턴입니다 */
	Basic,

	/** 사이클의 지정된 차례에만 사용하는 특수 패턴입니다 */
	Special
};

/**
 * 보스 전투의 진행 상태와 패턴 선택 정책을 소유합니다
 * Behavior Tree와 Blackboard를 참조하지 않으므로 AI 없이도 진행 규칙을 검증할 수 있습니다
 */
UCLASS(ClassGroup = "RS", meta = (BlueprintSpawnableComponent))
class RS_API URSBossPhaseComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URSBossPhaseComponent();

protected:
	/** 소유 보스의 체력 변경을 구독해 페이즈 트리거를 감시합니다 */
	virtual void BeginPlay() override;

	/** 체력 구독을 해제합니다 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	/** 지정한 Pawn이 소유한 진행 컴포넌트를 반환합니다 */
	static URSBossPhaseComponent* FindPhaseComponent(const APawn* Pawn);

#pragma region Pattern Cycle

	/** 현재 사이클 차례의 패턴 종류를 반환합니다 */
	bool TryGetCurrentPatternType(ERSBossPatternType& OutPatternType) const;

	/** 지정한 종류의 후보 중 사용할 어빌리티를 선택합니다 */
	bool TrySelectPattern(ERSBossPatternType PatternType, TSubclassOf<URSBaseGameplayAbility>& OutAbilityClass) const;

	/** 정상적으로 수행을 마친 패턴 하나만큼 사이클을 진행합니다 */
	void AdvancePatternCycle();

	/** 사이클을 처음 차례로 되돌립니다 */
	void ResetPatternCycle();

	/**
	 * 후보가 없는 차례를 건너뛰어 실행 가능한 차례로 맞춥니다
	 * 데이터 오류로 보스가 영구히 멈추지 않게 하는 안전망이며 모든 차례에 후보가 없으면 false를 반환합니다
	 */
	bool SkipUnusablePatternTurns();

	/** 현재 차례가 사이클 순서에서 몇 번째인지 반환합니다 */
	int32 GetCurrentCycleIndex() const { return CurrentCycleIndex; }

	/** 현재 페이즈의 한 사이클을 구성하는 행동 수를 반환합니다 */
	int32 GetCycleLength() const;

#pragma endregion

#pragma region Phase

	/** 현재 진행 중인 페이즈의 순서 번호를 반환합니다 */
	int32 GetCurrentPhaseIndex() const { return CurrentPhaseIndex; }

	/** 설정된 페이즈 개수를 반환합니다 */
	int32 GetPhaseCount() const;

	/** 체력 기준에 도달해 다음 페이즈로 넘어가기를 기다리는 중인지 반환합니다 */
	bool IsPhaseTransitionPending() const { return bPhaseTransitionPending; }

	/** 전환 전에 실행할 메인 기믹이 설정되어 있으면 반환하며 기믹 없는 전환에서는 false입니다 */
	bool TryGetPendingMainGimmick(TSubclassOf<URSBaseGameplayAbility>& OutAbilityClass) const;

	/** 다음 페이즈를 활성화하고 사이클과 기믹 대기 상태를 초기화합니다 */
	void AdvanceToNextPhase();

#pragma endregion

#if WITH_DEV_AUTOMATION_TESTS
	/** 자동 테스트가 페이즈 데이터를 애셋 없이 준비할 수 있게 합니다 */
	void SetPhasesForTest(const TArray<FRSBossPhaseDefinition>& InPhases);

	/** 자동 테스트가 체력 변경 경로를 직접 실행할 수 있게 합니다 */
	void EvaluateHealthTriggerForTest(float Health, float MaxHealth);
#endif

private:
	/** 현재 페이즈의 정의를 반환하며 설정이 없으면 nullptr을 반환합니다 */
	const FRSBossPhaseDefinition* GetCurrentPhase() const;

	/** 지정한 종류의 패턴 후보 목록을 반환합니다 */
	const TArray<TSubclassOf<URSBaseGameplayAbility>>* GetPatternCandidates(ERSBossPatternType PatternType) const;

	/** 소유 보스의 HealthComponent를 반환합니다 */
	URSHealthComponent* FindHealthComponent() const;

	/** 체력 변경마다 현재 페이즈의 트리거 도달을 확인합니다 */
	UFUNCTION()
	void HandleHealthChanged(URSHealthComponent* HealthComponent, float OldValue, float NewValue);

	/** 현재 페이즈의 체력 기준을 평가하고 한 번만 페이즈 전환 대기를 발행합니다 */
	void EvaluateHealthTrigger(float Health, float MaxHealth);

	/** 현재 페이즈 뒤에 넘어갈 페이즈가 있는지 반환합니다 */
	bool HasNextPhase() const;

protected:
	/** 이 보스의 페이즈 구성입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Boss|Phase")
	TObjectPtr<URSBossPhaseData> PhaseData;

private:
	/** 진행 중인 페이즈의 순서 번호입니다 */
	UPROPERTY(Transient)
	int32 CurrentPhaseIndex = 0;

	/** 현재 페이즈의 사이클 순서에서 현재 차례를 가리키는 위치입니다 */
	UPROPERTY(Transient)
	int32 CurrentCycleIndex = 0;

	/** 현재 페이즈의 체력 기준 도달이 이미 발행되었는지 나타냅니다 */
	UPROPERTY(Transient)
	bool bPhaseTransitionPending = false;

	/** 체력 구독을 해제하기 위해 보관한 HealthComponent입니다 */
	UPROPERTY(Transient)
	TObjectPtr<URSHealthComponent> ObservedHealthComponent;
};
