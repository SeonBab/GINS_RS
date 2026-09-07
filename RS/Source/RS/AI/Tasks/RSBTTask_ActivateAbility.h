// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "BehaviorTree/BTTaskNode.h"
#include "GameplayAbilitySpecHandle.h"
#include "RSBTTask_ActivateAbility.generated.h"

class URSAbilitySystemComponent;
class URSBaseGameplayAbility;

/**
 * Behavior Tree가 선택한 Gameplay Ability 하나를 활성화합니다
 * 넓은 분류 Tag로 여러 어빌리티를 한 번에 켜지 않도록 활성화 대상을 클래스로 지정합니다
 * 종료를 기다리는 동안 실행별 상태를 유지해야 하므로 노드 인스턴스를 사용합니다
 */
UCLASS()
class RS_API URSBTTask_ActivateAbility : public UBTTaskNode
{
	GENERATED_BODY()

public:
	URSBTTask_ActivateAbility();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;

private:
	/** 어빌리티 종료를 기다리며 등록한 델리게이트를 해제하고 이번 실행 상태를 정리합니다 */
	void ClearActivationState();

	/** 활성화한 어빌리티가 끝나면 대기 중이던 Task를 완료합니다 */
	void HandleAbilityEnded(const FAbilityEndedData& EndedData);

private:
	/** 활성화할 어빌리티 클래스이며 ASC에 부여된 스펙 중 이 클래스와 일치하는 하나를 찾습니다 */
	UPROPERTY(EditAnywhere, Category = "RS|Ability")
	TSubclassOf<URSBaseGameplayAbility> AbilityClass;

	/** 어빌리티가 끝날 때까지 Task를 유지할지 결정하며, 끄면 활성화 직후 성공합니다 */
	UPROPERTY(EditAnywhere, Category = "RS|Ability")
	bool bWaitForAbilityEnd = true;

	/** 종료를 기다리는 동안 결과를 돌려줄 Behavior Tree입니다 */
	UPROPERTY(Transient)
	TObjectPtr<UBehaviorTreeComponent> WaitingOwnerComp;

	/** 종료 알림을 등록한 ASC입니다 */
	UPROPERTY(Transient)
	TObjectPtr<URSAbilitySystemComponent> WaitingAbilitySystemComponent;

	/** 이번 실행에서 활성화한 어빌리티 스펙입니다 */
	FGameplayAbilitySpecHandle ActivatedHandle;

	/** 종료 알림을 받기 위해 등록한 델리게이트입니다 */
	FDelegateHandle AbilityEndedDelegateHandle;
};
