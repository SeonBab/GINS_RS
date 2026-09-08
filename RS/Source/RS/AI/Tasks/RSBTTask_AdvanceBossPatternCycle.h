// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "RSBTTask_AdvanceBossPatternCycle.generated.h"

/**
 * 보스의 패턴 사이클을 다음 차례로 진행시킵니다
 * Sequence의 마지막에 두어 어빌리티가 정상 종료된 패턴만 차례를 소비하게 합니다
 */
UCLASS()
class RS_API URSBTTask_AdvanceBossPatternCycle : public UBTTaskNode
{
	GENERATED_BODY()

public:
	URSBTTask_AdvanceBossPatternCycle();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;
};
