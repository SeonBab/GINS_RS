// Fill out your copyright notice in the Description page of Project Settings.

#include "RSBTTask_ActivateAbility.h"

#include "AIController.h"
#include "AbilitySystemGlobals.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "RSAbilitySystemComponent.h"
#include "Abilities/RSBaseGameplayAbility.h"

URSBTTask_ActivateAbility::URSBTTask_ActivateAbility()
{
	NodeName = TEXT("Activate Ability");

	// 종료를 기다리는 동안 실행별 핸들과 델리게이트를 유지해야 하므로 노드마다 인스턴스를 만듭니다
	bCreateNodeInstance = true;
}

EBTNodeResult::Type URSBTTask_ActivateAbility::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	ClearActivationState();

	if (!AbilityClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s has no ability class configured"), *GetName());

		return EBTNodeResult::Failed;
	}

	const AAIController* Controller = OwnerComp.GetAIOwner();
	APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;

	URSAbilitySystemComponent* AbilitySystemComponent = Cast<URSAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn));
	if (!AbilitySystemComponent)
	{
		return EBTNodeResult::Failed;
	}

	const FGameplayAbilitySpec* AbilitySpec = AbilitySystemComponent->FindAbilitySpecFromClass(AbilityClass);
	if (!AbilitySpec)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s could not find a granted spec for %s"), *GetName(), *GetNameSafe(AbilityClass));

		return EBTNodeResult::Failed;
	}

	const FGameplayAbilitySpecHandle Handle = AbilitySpec->Handle;

	// 활성화 실패는 쿨다운이나 상태 차단처럼 정상적인 상황이므로 Task 실패로 돌려 다른 가지를 선택하게 합니다
	if (!AbilitySystemComponent->TryActivateAbility(Handle))
	{
		return EBTNodeResult::Failed;
	}

	if (!bWaitForAbilityEnd)
	{
		return EBTNodeResult::Succeeded;
	}

	// 활성화가 같은 프레임에 끝난 어빌리티는 더 기다릴 것이 없습니다
	const FGameplayAbilitySpec* ActivatedSpec = AbilitySystemComponent->FindAbilitySpecFromHandle(Handle);
	if (!ActivatedSpec || !ActivatedSpec->IsActive())
	{
		return EBTNodeResult::Succeeded;
	}

	WaitingOwnerComp = &OwnerComp;
	WaitingAbilitySystemComponent = AbilitySystemComponent;
	ActivatedHandle = Handle;
	AbilityEndedDelegateHandle = AbilitySystemComponent->OnAbilityEnded.AddUObject(this, &ThisClass::HandleAbilityEnded);

	return EBTNodeResult::InProgress;
}

EBTNodeResult::Type URSBTTask_ActivateAbility::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (URSAbilitySystemComponent* AbilitySystemComponent = WaitingAbilitySystemComponent)
	{
		const FGameplayAbilitySpecHandle HandleToCancel = ActivatedHandle;

		// 델리게이트를 먼저 해제해야 취소로 발생하는 종료 알림이 이미 중단된 Task를 완료시키지 않습니다
		ClearActivationState();

		AbilitySystemComponent->CancelAbilityHandle(HandleToCancel);
	}
	else
	{
		ClearActivationState();
	}

	return EBTNodeResult::Aborted;
}

FString URSBTTask_ActivateAbility::GetStaticDescription() const
{
	return FString::Printf(TEXT("%s%s"), *GetNameSafe(AbilityClass), bWaitForAbilityEnd ? TEXT(" (종료까지 대기)") : TEXT(""));
}

void URSBTTask_ActivateAbility::ClearActivationState()
{
	if (WaitingAbilitySystemComponent && AbilityEndedDelegateHandle.IsValid())
	{
		WaitingAbilitySystemComponent->OnAbilityEnded.Remove(AbilityEndedDelegateHandle);
	}

	AbilityEndedDelegateHandle.Reset();
	ActivatedHandle = FGameplayAbilitySpecHandle();
	WaitingAbilitySystemComponent = nullptr;
	WaitingOwnerComp = nullptr;
}

void URSBTTask_ActivateAbility::HandleAbilityEnded(const FAbilityEndedData& EndedData)
{
	// 같은 ASC의 다른 어빌리티 종료가 이 Task를 끝내지 않게 이번 실행의 스펙만 받아들입니다
	if (EndedData.AbilitySpecHandle != ActivatedHandle)
	{
		return;
	}

	UBehaviorTreeComponent* OwnerComp = WaitingOwnerComp;
	const bool bWasCancelled = EndedData.bWasCancelled;

	ClearActivationState();

	if (!OwnerComp)
	{
		return;
	}

	FinishLatentTask(*OwnerComp, bWasCancelled ? EBTNodeResult::Failed : EBTNodeResult::Succeeded);
}
