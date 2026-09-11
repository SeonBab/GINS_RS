// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RSBaseGameplayAbility.h"
#include "RSGameplayAbility_BossGroggy.generated.h"

class UAnimMontage;

/**
 * 메인 기믹을 파훼당한 보스가 무력화되는 구간을 소유합니다
 * 별도의 지속 시간을 두지 않고 Montage 길이가 무력화 시간을 결정합니다
 */
UCLASS(Abstract, Blueprintable)
class RS_API URSGameplayAbility_BossGroggy : public URSBaseGameplayAbility
{
	GENERATED_BODY()

public:
	URSGameplayAbility_BossGroggy();

protected:
	/** 무력화 Montage를 재생하고 그 수명 동안 어빌리티를 유지합니다 */
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	/** 중단된 Montage가 남긴 애니메이션 Gameplay State를 정리합니다 */
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/** 무력화 Montage가 끝나면 어빌리티를 정상 종료합니다 */
	UFUNCTION()
	void HandleGroggyMontageFinished();

	/** 무력화 Montage가 중단되면 취소 종료합니다 */
	UFUNCTION()
	void HandleGroggyMontageCancelled();

protected:
	/** 무력화의 전체 수명과 시각적 동작을 결정하는 Montage입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Groggy")
	TObjectPtr<UAnimMontage> GroggyMontage;
};
