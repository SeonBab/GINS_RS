// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RSBaseGameplayAbility.h"
#include "RSBaseGameplayAbility_GetUp.generated.h"

class UAnimMontage;

/**
 * 누운 상태에서 일어나는 구간을 소유하는 기상 어빌리티의 기반입니다
 * 누운 상태 확인과 Commit이 모두 성공한 뒤에만 누움 어빌리티를 취소하여, 실패한 활성화가 누운 상태를 먼저 지우지 않게 합니다
 */
UCLASS(Abstract, Blueprintable)
class RS_API URSBaseGameplayAbility_GetUp : public URSBaseGameplayAbility
{
	GENERATED_BODY()

public:
	URSBaseGameplayAbility_GetUp();

protected:
	/** 누움을 종료시키고 기상 Montage를 재생합니다 */
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	/** 중단된 Montage가 남긴 애니메이션 Gameplay State를 정리합니다 */
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/** Commit이 끝난 뒤 파생 기상이 추가로 수행할 이동을 시작합니다 */
	virtual void StartGetUpMovement(ACharacter& Character);

	/** 기상 Montage가 끝나면 어빌리티를 정상 종료합니다 */
	UFUNCTION()
	void HandleGetUpMontageFinished();

	/** 기상 Montage가 중단되면 취소 종료합니다 */
	UFUNCTION()
	void HandleGetUpMontageCancelled();

protected:
	/** 기상의 전체 수명과 시각적 동작을 결정하는 Montage입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Get Up")
	TObjectPtr<UAnimMontage> GetUpMontage;
};
