// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RSBossPhaseComponent.generated.h"

class APawn;
class URSBaseGameplayAbility;

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

	/** 지정한 Pawn이 소유한 진행 컴포넌트를 반환합니다 */
	static URSBossPhaseComponent* FindPhaseComponent(const APawn* Pawn);

	/** 현재 사이클 차례의 패턴 종류를 반환합니다 */
	bool TryGetCurrentPatternType(ERSBossPatternType& OutPatternType) const;

	/** 지정한 종류의 후보 중 사용할 어빌리티를 선택합니다 */
	bool TrySelectPattern(ERSBossPatternType PatternType, TSubclassOf<URSBaseGameplayAbility>& OutAbilityClass) const;

	/** 정상적으로 수행을 마친 패턴 하나만큼 사이클을 진행합니다 */
	void AdvancePatternCycle();

	/** 사이클을 처음 차례로 되돌립니다 */
	void ResetPatternCycle();

	/** 현재 차례가 CycleSequence에서 몇 번째인지 반환합니다 */
	int32 GetCurrentCycleIndex() const { return CurrentCycleIndex; }

	/** 한 사이클을 구성하는 행동 수를 반환합니다 */
	int32 GetCycleLength() const { return CycleSequence.Num(); }

#if WITH_DEV_AUTOMATION_TESTS
	/** 자동 테스트가 사이클 구성과 후보 목록을 준비할 수 있게 합니다 */
	void SetPatternCycleForTest(const TArray<ERSBossPatternType>& InCycleSequence, const TArray<TSubclassOf<URSBaseGameplayAbility>>& InBasicPatterns, const TArray<TSubclassOf<URSBaseGameplayAbility>>& InSpecialPatterns);
#endif

private:
	/** 지정한 종류의 패턴 후보 목록을 반환합니다 */
	const TArray<TSubclassOf<URSBaseGameplayAbility>>& GetPatternCandidates(ERSBossPatternType PatternType) const;

protected:
	/** 상시 차례에 사용할 패턴 후보이며 이 중 하나를 무작위로 선택합니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Boss|Pattern")
	TArray<TSubclassOf<URSBaseGameplayAbility>> BasicPatterns;

	/** 특수 차례에 사용할 패턴 후보이며 이 중 하나를 무작위로 선택합니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Boss|Pattern")
	TArray<TSubclassOf<URSBaseGameplayAbility>> SpecialPatterns;

	/**
	 * 패턴 종류를 어떤 순서로 반복할지 정의합니다
	 * 순서만 다른 페이즈는 Behavior Tree를 바꾸지 않고 이 배열로 표현합니다
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Boss|Pattern")
	TArray<ERSBossPatternType> CycleSequence;

private:
	/** CycleSequence에서 현재 차례를 가리키는 위치입니다 */
	UPROPERTY(Transient)
	int32 CurrentCycleIndex = 0;
};
