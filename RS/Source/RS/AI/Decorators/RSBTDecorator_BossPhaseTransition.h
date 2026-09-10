// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTDecorator.h"
#include "RSBTDecorator_BossPhaseTransition.generated.h"

/**
 * 보스가 체력 기준에 도달해 다음 페이즈로 넘어가기를 기다리는지 판단합니다
 * 메인 기믹이 있는 전환과 없는 전환을 서로 다른 가지가 맡도록 조건을 나눕니다
 */
UCLASS()
class RS_API URSBTDecorator_BossPhaseTransition : public UBTDecorator
{
	GENERATED_BODY()

public:
	URSBTDecorator_BossPhaseTransition();

	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;
	virtual FString GetStaticDescription() const override;

protected:
	/**
	 * 켜면 메인 기믹이 설정된 전환에서만, 끄면 기믹이 없는 전환에서만 통과합니다
	 * 기믹 가지가 실패했을 때 기믹 없는 전환 가지가 대신 페이즈를 넘기지 않도록 두 조건을 배타로 둡니다
	 */
	UPROPERTY(EditAnywhere, Category = "RS|Boss")
	bool bRequireMainGimmick = false;
};
