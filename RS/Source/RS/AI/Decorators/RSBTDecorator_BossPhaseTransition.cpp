// Fill out your copyright notice in the Description page of Project Settings.


#include "RSBTDecorator_BossPhaseTransition.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Abilities/RSBaseGameplayAbility.h"
#include "RSBossPhaseComponent.h"

URSBTDecorator_BossPhaseTransition::URSBTDecorator_BossPhaseTransition()
{
	NodeName = TEXT("Boss Phase Transition");
}

bool URSBTDecorator_BossPhaseTransition::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
	const AAIController* Controller = OwnerComp.GetAIOwner();
	const URSBossPhaseComponent* PhaseComponent = URSBossPhaseComponent::FindPhaseComponent(Controller ? Controller->GetPawn() : nullptr);
	if (!PhaseComponent || !PhaseComponent->IsPhaseTransitionPending())
	{
		return false;
	}

	TSubclassOf<URSBaseGameplayAbility> PendingMainGimmick;
	const bool bHasMainGimmick = PhaseComponent->TryGetPendingMainGimmick(PendingMainGimmick);

	return bHasMainGimmick == bRequireMainGimmick;
}

FString URSBTDecorator_BossPhaseTransition::GetStaticDescription() const
{
	return bRequireMainGimmick ? TEXT("메인 기믹이 있는 페이즈 전환을 기다립니다") : TEXT("메인 기믹이 없는 페이즈 전환을 기다립니다");
}
