// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "RSBaseGameplayAbility.h"
#include "RSGameplayAbility_Knockdown.generated.h"

class UAnimMontage;
class URSAbilityTask_CurveMovement;

/**
 * 공격이 요청한 넉다운을 수행하며 밀려나 넘어지는 구간을 소유하는 어빌리티입니다
 * 넘어짐이 끝나면 누움 어빌리티로 상태를 넘기고, 넘기지 못하면 잠긴 상태를 남기지 않고 종료합니다
 */
UCLASS(Abstract, Blueprintable)
class RS_API URSGameplayAbility_Knockdown : public URSBaseGameplayAbility
{
	GENERATED_BODY()

public:
	URSGameplayAbility_Knockdown();

protected:
	/** 공격이 보낸 넉백 값으로 이동을 시작하고 넘어짐 Montage를 재생합니다 */
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	/** 남은 이동과 애니메이션 상태를 정리하고, 정상 완료였다면 누움으로 넘깁니다 */
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/** 넘어짐 Montage가 끝나면 누움으로 넘기며 종료합니다 */
	UFUNCTION()
	void HandleKnockdownMontageCompleted();

	/** 넘어짐 Montage가 중단되면 취소 종료합니다 */
	UFUNCTION()
	void HandleKnockdownMontageCancelled();

protected:
	/** 넘어지는 구간의 수명과 시각적 동작을 결정하는 Montage입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Knockdown")
	TObjectPtr<UAnimMontage> KnockdownMontage;

	/** 넘어짐이 끝난 뒤 상태를 이어받을 누움 어빌리티입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Knockdown")
	TSubclassOf<URSBaseGameplayAbility> DownedAbilityClass;

	/** 정규화 시간 0~1을 전체 넉백 거리의 누적 진행률 0~1로 변환합니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Knockdown")
	FRuntimeFloatCurve DistanceProgressCurve;

private:
	/** 현재 실행에서 넉백 이동을 담당하는 AbilityTask입니다 */
	UPROPERTY(Transient)
	TObjectPtr<URSAbilityTask_CurveMovement> ActiveKnockbackTask;

	/** 넘어짐이 정상 완료되어 종료 시 누움으로 넘겨야 하는지 나타냅니다 */
	bool bTransitionToDowned = false;
};
