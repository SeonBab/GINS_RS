// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "RSBossPhaseComponent.h"
#include "RSBTTask_SelectBossPattern.generated.h"

/**
 * 보스 진행 컴포넌트에 패턴 선택을 요청하고 결과를 Blackboard에 기록합니다
 * 선택 정책은 컴포넌트가 소유하므로 이 Task는 Behavior Tree와 컴포넌트를 연결하는 역할만 합니다
 */
UCLASS()
class RS_API URSBTTask_SelectBossPattern : public UBTTaskNode
{
	GENERATED_BODY()

public:
	URSBTTask_SelectBossPattern();

	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;

protected:
	/** 이 노드가 선택할 패턴의 종류입니다 */
	UPROPERTY(EditAnywhere, Category = "RS|Boss")
	ERSBossPatternType PatternType = ERSBossPatternType::Basic;

	/** 선택한 어빌리티 클래스를 기록할 Blackboard 키입니다 */
	UPROPERTY(EditAnywhere, Category = "RS|Boss")
	FBlackboardKeySelector SelectedAbilityKey;
};
