// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTDecorator.h"
#include "RSBossPhaseComponent.h"
#include "RSBTDecorator_BossPatternType.generated.h"

/**
 * 보스의 현재 사이클 차례가 설정한 패턴 종류인지 판단합니다
 * 사이클 상태는 Blackboard에 복제하지 않으므로 진행 컴포넌트를 직접 조회합니다
 */
UCLASS()
class RS_API URSBTDecorator_BossPatternType : public UBTDecorator
{
	GENERATED_BODY()

public:
	URSBTDecorator_BossPatternType();

	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;
	virtual FString GetStaticDescription() const override;

protected:
	/** 이 가지가 담당하는 패턴의 종류입니다 */
	UPROPERTY(EditAnywhere, Category = "RS|Boss")
	ERSBossPatternType ExpectedPatternType = ERSBossPatternType::Basic;
};
