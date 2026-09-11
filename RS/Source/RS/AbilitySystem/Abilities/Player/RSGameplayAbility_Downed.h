// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RSBaseGameplayAbility.h"
#include "RSGameplayAbility_Downed.generated.h"

class UAnimMontage;

/**
 * 넘어짐이 끝난 뒤 누워 기상을 기다리는 구간을 소유하는 어빌리티입니다
 * 누운 자세는 넘어짐 Montage와 같은 슬롯에서 이어지도록 루프 Montage로 유지하고,
 * 행동 잠금은 루프마다 Notify State가 다시 시작되지 않도록 이 어빌리티가 직접 소유합니다
 */
UCLASS(Abstract, Blueprintable)
class RS_API URSGameplayAbility_Downed : public URSBaseGameplayAbility
{
	GENERATED_BODY()

public:
	URSGameplayAbility_Downed();

protected:
	/** 누운 자세를 재생하고 자동 기상까지 남은 시간을 기다립니다 */
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	/** 중단된 Montage가 남긴 애니메이션 Gameplay State를 정리합니다 */
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/** 대기가 끝나면 일반 기상으로 넘기며 종료합니다 */
	UFUNCTION()
	void HandleAutoGetUpDelayFinished();

protected:
	/**
	 * 누운 자세를 유지할 Montage이며 마지막 섹션이 자기 자신을 다음 섹션으로 가리켜 반복합니다
	 * 넘어짐 Montage와 같은 슬롯에서 이어지므로 상태 머신으로 빠졌다 돌아오는 이음매가 없습니다
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Downed")
	TObjectPtr<UAnimMontage> DownedMontage;

	/** 입력이 없을 때 스스로 일어나기까지 누워 있을 시간입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Downed", meta = (ClampMin = "0.01", UIMin = "0.01", ForceUnits = "s"))
	float AutoGetUpDelay = 1.5f;

	/** 대기가 끝나면 실행할 일반 기상 어빌리티입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Downed")
	TSubclassOf<URSBaseGameplayAbility> NormalGetUpAbilityClass;
};
