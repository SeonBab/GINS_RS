// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "RSAbilityTask_WaitMontageHeldAtSectionEnd.generated.h"

class UAnimMontage;

/** Montage가 지정 Section의 마지막 Pose를 유지하기 시작하면 발생합니다 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRSMontageHeldAtSectionEnd);

/** Montage가 지정 Section의 마지막 Pose에 도달하지 못하고 유효하지 않게 되면 발생합니다 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRSMontageHoldInvalidated);

/** Auto Blend Out이 꺼진 Montage가 지정 Section 끝에서 마지막 Pose를 유지하는 상태를 관찰합니다 */
UCLASS()
class RS_API URSAbilityTask_WaitMontageHeldAtSectionEnd : public UAbilityTask
{
	GENERATED_BODY()

public:
	URSAbilityTask_WaitMontageHeldAtSectionEnd(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 지정 Section의 마지막 Pose 유지 상태에 정상적으로 도달하면 한 번 발생합니다 */
	UPROPERTY(BlueprintAssignable, Category = "RS|Animation")
	FRSMontageHeldAtSectionEnd OnCompleted;

	/** Montage가 중단되거나 실행 계약이 유효하지 않으면 한 번 발생합니다 */
	UPROPERTY(BlueprintAssignable, Category = "RS|Animation")
	FRSMontageHoldInvalidated OnInvalidated;

	/** 지정 Montage가 Section 끝에서 마지막 Pose를 유지할 때까지 기다리는 AbilityTask를 생성합니다 */
	static URSAbilityTask_WaitMontageHeldAtSectionEnd* WaitMontageHeldAtSectionEnd(UGameplayAbility* OwningAbility, UAnimMontage* Montage, FName SectionName);

	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;

private:
	/** 실패 이벤트를 한 번 전달하고 Task를 종료합니다 */
	void InvalidateTask();

	/** 관찰할 Montage입니다 */
	TObjectPtr<UAnimMontage> MontageToObserve;

	/** 마지막 Pose 유지 상태를 기다릴 Section입니다 */
	FName ExpectedSectionName = NAME_None;
};
