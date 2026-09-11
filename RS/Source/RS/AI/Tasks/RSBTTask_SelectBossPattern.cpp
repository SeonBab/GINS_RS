// Fill out your copyright notice in the Description page of Project Settings.


#include "RSBTTask_SelectBossPattern.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Class.h"
#include "Abilities/RSBaseGameplayAbility.h"

URSBTTask_SelectBossPattern::URSBTTask_SelectBossPattern()
{
	NodeName = TEXT("Select Boss Pattern");

	SelectedAbilityKey.AddClassFilter(this, GET_MEMBER_NAME_CHECKED(URSBTTask_SelectBossPattern, SelectedAbilityKey), URSBaseGameplayAbility::StaticClass());
}

void URSBTTask_SelectBossPattern::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);

	// Blackboard 애셋과 연결하지 않으면 선택한 키의 ID가 해석되지 않습니다
	if (const UBlackboardData* BlackboardAsset = GetBlackboardAsset())
	{
		SelectedAbilityKey.ResolveSelectedKey(*BlackboardAsset);
	}
}

EBTNodeResult::Type URSBTTask_SelectBossPattern::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Blackboard || SelectedAbilityKey.SelectedKeyType != UBlackboardKeyType_Class::StaticClass())
	{
		return EBTNodeResult::Failed;
	}

	// 선택에 실패했을 때 직전 어빌리티가 남아 다시 실행되지 않도록 기록 전에 먼저 비웁니다
	Blackboard->SetValue<UBlackboardKeyType_Class>(SelectedAbilityKey.GetSelectedKeyID(), nullptr);

	const AAIController* Controller = OwnerComp.GetAIOwner();
	URSBossPhaseComponent* PhaseComponent = URSBossPhaseComponent::FindPhaseComponent(Controller ? Controller->GetPawn() : nullptr);
	if (!PhaseComponent)
	{
		return EBTNodeResult::Failed;
	}

	TSubclassOf<URSBaseGameplayAbility> SelectedAbilityClass;
	if (!PhaseComponent->TrySelectPattern(PatternType, SelectedAbilityClass))
	{
		return EBTNodeResult::Failed;
	}

	Blackboard->SetValue<UBlackboardKeyType_Class>(SelectedAbilityKey.GetSelectedKeyID(), SelectedAbilityClass.Get());

	// 다음 Activate Ability Task의 성공 여부와 무관하게 특수 패턴 진입 시점에서 지속 오브젝트 수명을 먼저 진행합니다
	if (PatternType == ERSBossPatternType::Special)
	{
		PhaseComponent->NotifyAbilityActivationRequested(SelectedAbilityClass);
	}

	return EBTNodeResult::Succeeded;
}

FString URSBTTask_SelectBossPattern::GetStaticDescription() const
{
	return FString::Printf(TEXT("%s -> %s"), *UEnum::GetDisplayValueAsText(PatternType).ToString(), *SelectedAbilityKey.SelectedKeyName.ToString());
}
