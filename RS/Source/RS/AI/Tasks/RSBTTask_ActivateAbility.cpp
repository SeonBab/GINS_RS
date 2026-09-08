// Fill out your copyright notice in the Description page of Project Settings.

#include "RSBTTask_ActivateAbility.h"

#include "AIController.h"
#include "AbilitySystemGlobals.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Class.h"
#include "RSAbilitySystemComponent.h"
#include "Abilities/RSBaseGameplayAbility.h"

URSBTTask_ActivateAbility::URSBTTask_ActivateAbility()
{
	NodeName = TEXT("Activate Ability");

	// 종료를 기다리는 동안 실행별 핸들과 델리게이트를 유지해야 하므로 노드마다 인스턴스를 만듭니다
	bCreateNodeInstance = true;

	// 고정 클래스를 사용하는 기존 구성과 함께 쓰이므로 키를 비워 두는 것도 정상 설정입니다
	AbilityClassKey.AllowNoneAsValue(true);
	AbilityClassKey.AddClassFilter(this, GET_MEMBER_NAME_CHECKED(URSBTTask_ActivateAbility, AbilityClassKey), URSBaseGameplayAbility::StaticClass());
}

void URSBTTask_ActivateAbility::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);

	// Blackboard 애셋과 연결하지 않으면 선택한 키의 ID가 해석되지 않습니다
	if (const UBlackboardData* BlackboardAsset = GetBlackboardAsset())
	{
		AbilityClassKey.ResolveSelectedKey(*BlackboardAsset);
	}
}

EBTNodeResult::Type URSBTTask_ActivateAbility::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	ClearActivationState();

	const TSubclassOf<URSBaseGameplayAbility> AbilityClassToActivate = ResolveAbilityClass(OwnerComp);
	if (!AbilityClassToActivate)
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

	const FGameplayAbilitySpec* AbilitySpec = AbilitySystemComponent->FindAbilitySpecFromClass(AbilityClassToActivate);
	if (!AbilitySpec)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s could not find a granted spec for %s"), *GetName(), *GetNameSafe(AbilityClassToActivate));

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
	const bool bUsesBlackboardKey = !AbilityClassKey.IsNone() && AbilityClassKey.SelectedKeyName != NAME_None;
	const FString AbilitySourceDescription = bUsesBlackboardKey ? AbilityClassKey.SelectedKeyName.ToString() : GetNameSafe(AbilityClass);

	return FString::Printf(TEXT("%s%s"), *AbilitySourceDescription, bWaitForAbilityEnd ? TEXT(" (종료까지 대기)") : TEXT(""));
}

TSubclassOf<URSBaseGameplayAbility> URSBTTask_ActivateAbility::ResolveAbilityClass(const UBehaviorTreeComponent& OwnerComp) const
{
	// 키를 지정하지 않은 기존 구성은 노드에 고정된 클래스를 그대로 사용합니다
	if (AbilityClassKey.SelectedKeyType != UBlackboardKeyType_Class::StaticClass())
	{
		return AbilityClass;
	}

	const UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Blackboard)
	{
		return AbilityClass;
	}

	UClass* SelectedClass = Blackboard->GetValue<UBlackboardKeyType_Class>(AbilityClassKey.GetSelectedKeyID());

	// 키의 클래스 필터는 에디터에서만 적용되므로 실행 시점에 한 번 더 확인합니다
	if (!SelectedClass || !SelectedClass->IsChildOf(URSBaseGameplayAbility::StaticClass()))
	{
		return nullptr;
	}

	TSubclassOf<URSBaseGameplayAbility> ResolvedAbilityClass;
	ResolvedAbilityClass = SelectedClass;

	return ResolvedAbilityClass;
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
