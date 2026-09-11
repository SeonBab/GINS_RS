// Fill out your copyright notice in the Description page of Project Settings.


#include "RSBTTask_SelectBossPhaseAbility.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Class.h"
#include "Abilities/RSBaseGameplayAbility.h"
#include "RSBossPhaseComponent.h"

URSBTTask_SelectBossPhaseAbility::URSBTTask_SelectBossPhaseAbility()
{
	NodeName = TEXT("Select Boss Phase Ability");

	SelectedAbilityKey.AddClassFilter(this, GET_MEMBER_NAME_CHECKED(URSBTTask_SelectBossPhaseAbility, SelectedAbilityKey), URSBaseGameplayAbility::StaticClass());
}

void URSBTTask_SelectBossPhaseAbility::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);

	// Blackboard 애셋과 연결하지 않으면 선택한 키의 ID가 해석되지 않습니다
	if (const UBlackboardData* BlackboardAsset = GetBlackboardAsset())
	{
		SelectedAbilityKey.ResolveSelectedKey(*BlackboardAsset);
	}
}

EBTNodeResult::Type URSBTTask_SelectBossPhaseAbility::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Blackboard || SelectedAbilityKey.SelectedKeyType != UBlackboardKeyType_Class::StaticClass())
	{
		return EBTNodeResult::Failed;
	}

	// 조건을 만족하지 않을 때 직전 어빌리티가 남아 다시 실행되지 않도록 기록 전에 먼저 비웁니다
	Blackboard->SetValue<UBlackboardKeyType_Class>(SelectedAbilityKey.GetSelectedKeyID(), nullptr);

	const AAIController* Controller = OwnerComp.GetAIOwner();
	URSBossPhaseComponent* PhaseComponent = URSBossPhaseComponent::FindPhaseComponent(Controller ? Controller->GetPawn() : nullptr);
	if (!PhaseComponent)
	{
		return EBTNodeResult::Failed;
	}

	TSubclassOf<URSBaseGameplayAbility> SelectedAbilityClass;
	const bool bHasAbility = AbilitySlot == ERSBossPhaseAbilitySlot::Groggy
		? PhaseComponent->TryGetPendingGroggy(SelectedAbilityClass)
		: PhaseComponent->TryGetPendingMainGimmick(SelectedAbilityClass);

	// 조건을 만족하지 않는 것은 정상이므로 빈 값을 남긴 채 성공합니다
	// 실제로 필요한 어빌리티인지는 실행 Task의 bSucceedWhenAbilityMissing이 판단합니다
	if (!bHasAbility)
	{
		return EBTNodeResult::Succeeded;
	}

	Blackboard->SetValue<UBlackboardKeyType_Class>(SelectedAbilityKey.GetSelectedKeyID(), SelectedAbilityClass.Get());

	// 메인 기믹 정리는 다음 Activate Ability Task가 실패해도 되돌리지 않도록 선택 직후 먼저 요청합니다
	if (AbilitySlot == ERSBossPhaseAbilitySlot::MainGimmick)
	{
		PhaseComponent->NotifyAbilityActivationRequested(SelectedAbilityClass);
	}

	return EBTNodeResult::Succeeded;
}

FString URSBTTask_SelectBossPhaseAbility::GetStaticDescription() const
{
	return FString::Printf(TEXT("%s -> %s"), *UEnum::GetDisplayValueAsText(AbilitySlot).ToString(), *SelectedAbilityKey.SelectedKeyName.ToString());
}
