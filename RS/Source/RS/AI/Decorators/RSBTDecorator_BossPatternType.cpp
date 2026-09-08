// Fill out your copyright notice in the Description page of Project Settings.


#include "RSBTDecorator_BossPatternType.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"

URSBTDecorator_BossPatternType::URSBTDecorator_BossPatternType()
{
	NodeName = TEXT("Boss Pattern Type");
}

bool URSBTDecorator_BossPatternType::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
	const AAIController* Controller = OwnerComp.GetAIOwner();
	const URSBossPhaseComponent* PhaseComponent = URSBossPhaseComponent::FindPhaseComponent(Controller ? Controller->GetPawn() : nullptr);
	if (!PhaseComponent)
	{
		return false;
	}

	ERSBossPatternType CurrentPatternType;
	if (!PhaseComponent->TryGetCurrentPatternType(CurrentPatternType))
	{
		return false;
	}

	return CurrentPatternType == ExpectedPatternType;
}

FString URSBTDecorator_BossPatternType::GetStaticDescription() const
{
	return FString::Printf(TEXT("현재 차례가 %s입니다"), *UEnum::GetDisplayValueAsText(ExpectedPatternType).ToString());
}
