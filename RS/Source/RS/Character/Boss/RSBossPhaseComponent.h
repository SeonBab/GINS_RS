// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RSBossPhaseComponent.generated.h"

class APawn;
class UGameplayAbility;
class URSBaseGameplayAbility;
class URSBossPhaseData;
class URSHealthComponent;

struct FRSBossPhaseDefinition;

/** 특수 패턴 활성화 시도 순번을 지속 오브젝트에 전달합니다 */
DECLARE_MULTICAST_DELEGATE_OneParam(FRSSpecialPatternActivationRequestedSignature, int32);

/** 메인 패턴, 보스 사망과 Encounter 종료가 지속 오브젝트의 즉시 정리를 요청합니다 */
DECLARE_MULTICAST_DELEGATE(FRSPersistentObjectCleanupRequestedSignature);

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
 * 메인 기믹에 대한 플레이어의 대응 결과입니다
 * 어빌리티가 정상 종료했는지와는 다른 축이며 게임플레이 판정만 나타냅니다
 */
UENUM(BlueprintType)
enum class ERSBossMainGimmickOutcome : uint8
{
	/** 아직 판정이 없습니다. 취소되어 판정에 도달하지 못한 경우를 포함합니다 */
	None,

	/** 플레이어가 기믹을 파훼했습니다 */
	Broken,

	/** 플레이어가 끝까지 기믹을 파훼하지 못했습니다 */
	NotBroken
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

	/** 지금까지 활성화를 요청한 특수 패턴의 순번을 반환합니다 */
	int32 GetSpecialPatternActivationSequence() const { return SpecialPatternActivationSequence; }

	/** 특수 패턴 활성화 시도를 구독할 델리게이트를 반환합니다 */
	FRSSpecialPatternActivationRequestedSignature& OnSpecialPatternActivationRequested() { return SpecialPatternActivationRequestedEvent; }

	/** Ability 활성화 직전에 현재 페이즈 기준으로 특수 패턴 순번 또는 메인 기믹 정리를 발행합니다 */
	void NotifyAbilityActivationRequested(TSubclassOf<URSBaseGameplayAbility> AbilityClass);

	/** 메인 패턴과 Encounter 수명 종료의 즉시 정리 요청을 구독할 델리게이트를 반환합니다 */
	FRSPersistentObjectCleanupRequestedSignature& OnPersistentObjectCleanupRequested() { return PersistentObjectCleanupRequestedEvent; }

	/** 보스 사망 또는 Encounter 종료가 모든 지속 오브젝트의 정리를 요청합니다 */
	void RequestPersistentObjectCleanup();

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

	/**
	 * 기믹을 파훼했고 무력화가 설정되어 있을 때만 무력화 어빌리티를 반환합니다
	 * 무력화를 실행할지 판단하는 조건을 이 함수 하나로 모읍니다
	 */
	bool TryGetPendingGroggy(TSubclassOf<URSBaseGameplayAbility>& OutAbilityClass) const;

	/**
	 * 보스 패턴이 종료되기 전에 자신의 파훼 판정 결과를 보고합니다
	 * 모든 패턴이 보고하므로 이번 페이즈의 메인 기믹이 낸 보고만 수락하고 나머지는 버립니다
	 * 보고자가 자신이 기믹인지 알 필요가 없도록 판별을 이 함수 하나로 모읍니다
	 */
	void ReportMainGimmickOutcome(const UGameplayAbility* Reporter, ERSBossMainGimmickOutcome Outcome);

	/** 현재 페이즈 기믹에 대한 판정 결과를 반환합니다 */
	ERSBossMainGimmickOutcome GetMainGimmickOutcome() const { return MainGimmickOutcome; }

	/** 다음 페이즈를 활성화하고 사이클과 기믹 대기 상태를 초기화합니다 */
	void AdvanceToNextPhase();

	/** 현재 페이즈에 설정된 전투 지속형 Ability를 중복 없이 활성화합니다 */
	bool EnterCurrentPhase();

#pragma endregion

#if WITH_DEV_AUTOMATION_TESTS
	/** 자동 테스트가 페이즈 데이터를 애셋 없이 준비할 수 있게 합니다 */
	void SetPhasesForTest(const TArray<FRSBossPhaseDefinition>& InPhases);

	/** 자동 테스트가 체력 변경 경로를 직접 실행할 수 있게 합니다 */
	void EvaluateHealthTriggerForTest(float Health, float MaxHealth);

	/** 자동 테스트가 공용 무력화 어빌리티를 지정할 수 있게 합니다 */
	void SetGroggyAbilityForTest(TSubclassOf<URSBaseGameplayAbility> InGroggyAbility);
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

	/** 활성화를 요청한 특수 패턴마다 증가하며 지속 오브젝트 묶음의 수명을 구분합니다 */
	UPROPERTY(Transient)
	int32 SpecialPatternActivationSequence = 0;

	/** 현재 페이즈의 체력 기준 도달이 이미 발행되었는지 나타냅니다 */
	UPROPERTY(Transient)
	bool bPhaseTransitionPending = false;

	/** 현재 페이즈 기믹에 대한 플레이어의 대응 결과입니다 */
	UPROPERTY(Transient)
	ERSBossMainGimmickOutcome MainGimmickOutcome = ERSBossMainGimmickOutcome::None;

	/** 체력 구독을 해제하기 위해 보관한 HealthComponent입니다 */
	UPROPERTY(Transient)
	TObjectPtr<URSHealthComponent> ObservedHealthComponent;

	/** 지속 오브젝트가 수명 순번을 판단하는 특수 패턴 활성화 요청 이벤트입니다 */
	FRSSpecialPatternActivationRequestedSignature SpecialPatternActivationRequestedEvent;

	/** 순번과 관계없이 모든 지속 오브젝트를 즉시 정리하는 이벤트입니다 */
	FRSPersistentObjectCleanupRequestedSignature PersistentObjectCleanupRequestedEvent;
};
