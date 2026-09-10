// Fill out your copyright notice in the Description page of Project Settings.


#include "RSBTTask_AdvanceBossPhase.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "RSBossPhaseComponent.h"

URSBTTask_AdvanceBossPhase::URSBTTask_AdvanceBossPhase()
{
	NodeName = TEXT("Advance Boss Phase");
}

EBTNodeResult::Type URSBTTask_AdvanceBossPhase::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	const AAIController* Controller = OwnerComp.GetAIOwner();
	URSBossPhaseComponent* PhaseComponent = URSBossPhaseComponent::FindPhaseComponent(Controller ? Controller->GetPawn() : nullptr);
	if (!PhaseComponent)
	{
		return EBTNodeResult::Failed;
	}

	PhaseComponent->AdvanceToNextPhase();

	return EBTNodeResult::Succeeded;
}

FString URSBTTask_AdvanceBossPhase::GetStaticDescription() const
{
	return TEXT("다음 페이즈를 활성화하고 사이클을 처음으로 되돌립니다");
}
