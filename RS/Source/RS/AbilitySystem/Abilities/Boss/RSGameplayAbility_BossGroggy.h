// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RSBaseGameplayAbility.h"
#include "RSGameplayAbility_BossGroggy.generated.h"

class UAnimMontage;

#if WITH_EDITOR
class FDataValidationContext;
#endif

/**
 * 메인 기믹을 파훼당한 보스가 무력화되는 구간을 소유합니다
 * 설정 시간을 넘지 않도록 Start와 완전한 Loop 반복 및 End를 재생합니다
 */
UCLASS(Abstract, Blueprintable)
class RS_API URSGameplayAbility_BossGroggy : public URSBaseGameplayAbility
{
	GENERATED_BODY()

public:
	URSGameplayAbility_BossGroggy();

	/** 요청 시간을 넘기지 않는 완전 Loop 횟수와 실제 무력화 시간을 계산합니다 */
	static bool TryCalculateGroggyTiming(float RequestedDuration, float StartDuration, float LoopDuration, float EndDuration, int32& OutLoopCount, float& OutEffectiveDuration);

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

	/** 마지막 Loop가 끝나면 End로 진행하도록 다음 Section을 바꿉니다 */
	UFUNCTION()
	void HandlePrepareFinalLoop();

#if WITH_EDITOR
	/** Montage Section과 전체 무력화 시간이 실행 계약에 맞는지 검사합니다 */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

protected:
	/** 무력화의 전체 수명과 시각적 동작을 결정하는 Montage입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Groggy")
	TObjectPtr<UAnimMontage> GroggyMontage;

	/** Section 길이를 실제 재생 시간으로 환산할 Montage 재생 속도입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Groggy", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float GroggyMontagePlayRate = 1.0f;

	/** Start와 완전 Loop 및 End를 합쳐 넘기지 않을 전체 무력화 시간입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Groggy", meta = (ClampMin = "0.01", UIMin = "0.01", ForceUnits = "s"))
	float GroggyDuration = 10.0f;

private:
	/** Montage Task 종료 Callback이 EndAbility에 재진입하지 않게 막습니다 */
	bool bIsEndingAbility = false;
};
