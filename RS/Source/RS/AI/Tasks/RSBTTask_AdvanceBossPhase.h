// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "RSBTTask_AdvanceBossPhase.generated.h"

/**
 * 보스를 다음 페이즈로 넘기고 사이클과 전환 대기를 초기화합니다
 * 전환 연출이나 무력화가 필요한 경우 그 노드들 뒤에 두어 마지막에 실행합니다
 */
UCLASS()
class RS_API URSBTTask_AdvanceBossPhase : public UBTTaskNode
{
	GENERATED_BODY()

public:
	URSBTTask_AdvanceBossPhase();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;
};
