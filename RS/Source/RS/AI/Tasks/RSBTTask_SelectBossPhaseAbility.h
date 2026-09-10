// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "RSBTTask_SelectBossPhaseAbility.generated.h"

/** 페이즈 전환 시퀀스에서 실행할 어빌리티의 종류입니다 */
UENUM()
enum class ERSBossPhaseAbilitySlot : uint8
{
	/** 체력 기준에 도달해 대기 중인 메인 기믹입니다 */
	MainGimmick,

	/** 기믹을 파훼했을 때 재생할 무력화입니다 */
	Groggy
};

/**
 * 페이즈 전환에 사용할 어빌리티를 진행 컴포넌트에서 받아 Blackboard에 기록합니다
 * 실행 조건 판단은 컴포넌트가 하고 이 Task는 결과를 옮기기만 합니다
 */
UCLASS()
class RS_API URSBTTask_SelectBossPhaseAbility : public UBTTaskNode
{
	GENERATED_BODY()

public:
	URSBTTask_SelectBossPhaseAbility();

	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;

protected:
	/** 이 노드가 기록할 어빌리티의 종류입니다 */
	UPROPERTY(EditAnywhere, Category = "RS|Boss")
	ERSBossPhaseAbilitySlot AbilitySlot = ERSBossPhaseAbilitySlot::MainGimmick;

	/** 선택한 어빌리티 클래스를 기록할 Blackboard 키입니다 */
	UPROPERTY(EditAnywhere, Category = "RS|Boss")
	FBlackboardKeySelector SelectedAbilityKey;
};
