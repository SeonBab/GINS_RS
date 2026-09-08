// Fill out your copyright notice in the Description page of Project Settings.


#include "RSBTTask_AdvanceBossPatternCycle.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "RSBossPhaseComponent.h"

URSBTTask_AdvanceBossPatternCycle::URSBTTask_AdvanceBossPatternCycle()
{
	NodeName = TEXT("Advance Boss Pattern Cycle");
}

EBTNodeResult::Type URSBTTask_AdvanceBossPatternCycle::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	const AAIController* Controller = OwnerComp.GetAIOwner();
	URSBossPhaseComponent* PhaseComponent = URSBossPhaseComponent::FindPhaseComponent(Controller ? Controller->GetPawn() : nullptr);
	if (!PhaseComponent)
	{
		return EBTNodeResult::Failed;
	}

	PhaseComponent->AdvancePatternCycle();

	return EBTNodeResult::Succeeded;
}

FString URSBTTask_AdvanceBossPatternCycle::GetStaticDescription() const
{
	return TEXT("정상 종료한 패턴만큼 사이클을 진행합니다");
}
