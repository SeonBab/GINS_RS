// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RSBaseGameplayAbility.h"
#include "RSGameplayAbility_HitReact.generated.h"

class UAnimMontage;

/**
 * 공격이 요청한 피격 경직을 수행하는 어빌리티입니다
 * 경직 상태는 이 어빌리티가 소유하고 행동 잠금과 면역은 Montage 구간의 Notify State가 소유합니다
 */
UCLASS(Abstract, Blueprintable)
class RS_API URSGameplayAbility_HitReact : public URSBaseGameplayAbility
{
	GENERATED_BODY()

public:
	URSGameplayAbility_HitReact();

protected:
	/** 진행 중인 이동을 멈추고 경직 Montage를 재생합니다 */
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	/** 중단된 Montage가 남긴 애니메이션 Gameplay State를 정리합니다 */
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/** 경직 Montage가 끝나면 어빌리티를 정상 종료합니다 */
	UFUNCTION()
	void HandleHitReactMontageFinished();

	/** 경직 Montage가 중단되면 취소 종료합니다 */
	UFUNCTION()
	void HandleHitReactMontageCancelled();

protected:
	/** 경직의 전체 수명과 시각적 동작을 결정하는 Montage입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Hit React")
	TObjectPtr<UAnimMontage> HitReactMontage;
};
