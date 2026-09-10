// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "Curves/CurveFloat.h"
#include "RSBaseGameplayAbility.h"
#include "RSGameplayAbility_Knockdown.generated.h"

class UAnimMontage;

#if WITH_EDITOR
class FDataValidationContext;
#endif

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

	/** 요청 시간을 넘기는 최소 완전 AirLoop 횟수와 실제 공중 시간을 계산합니다 */
	static bool TryCalculateAirborneTiming(float RequestedDuration, float StartDuration, float AirLoopDuration, float FallDuration, int32& OutAirLoopCount, float& OutEffectiveAirborneDuration);

protected:
	/** 공격이 보낸 넉백 값으로 이동을 시작하고 넘어짐 Montage를 재생합니다 */
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	/** 남은 이동과 애니메이션 상태를 정리합니다 */
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

#if WITH_EDITOR
	/** Knockdown Montage의 네 Section과 재생 설정이 실행 계약에 맞는지 검사합니다 */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	/** 마지막 AirLoop가 끝나면 Fall로 진행하도록 다음 Section을 바꿉니다 */
	UFUNCTION()
	void HandlePrepareFinalAirLoop();

	/** 이동과 공중 아치가 끝난 시점을 지면 접촉으로 보고 Land를 시작합니다 */
	UFUNCTION()
	void HandleKnockbackMovementCompleted();

	/** Land 마지막 Pose 유지 상태에 도달하면 누움으로 전환합니다 */
	UFUNCTION()
	void HandleKnockdownLandCompleted();

	/** 정상 누움 전환 외의 경로에서 넘어짐 Montage가 중단되면 취소 종료합니다 */
	UFUNCTION()
	void HandleKnockdownMontageCancelled();

	/** Land 구간의 빠른 기상 가용 상태를 한 번 적용합니다 */
	void ApplyQuickGetUpAvailability();

	/** 현재 실행이 적용한 빠른 기상 가용 상태를 회수합니다 */
	void ClearQuickGetUpAvailability(const FGameplayAbilityActorInfo* ActorInfo);

protected:
	/** 넘어지는 구간의 수명과 시각적 동작을 결정하는 Montage입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Knockdown")
	TObjectPtr<UAnimMontage> KnockdownMontage;

	/** Section 길이를 실제 재생 시간으로 환산할 Knockdown Montage 재생 속도입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Knockdown", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float KnockdownMontagePlayRate = 1.0f;

	/** 수평 거리가 없더라도 Knockdown의 공중 동작이 보이도록 보장하는 Mesh 아치 최소 높이입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Knockdown", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float MinimumKnockbackHeight = 80.0f;

	/** 넘어짐이 끝난 뒤 상태를 이어받을 누움 어빌리티입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Knockdown")
	TSubclassOf<URSBaseGameplayAbility> DownedAbilityClass;

	/** 정규화 시간 0~1을 전체 넉백 거리의 누적 진행률 0~1로 변환합니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Knockdown")
	FRuntimeFloatCurve DistanceProgressCurve;

private:
	/** 이동 완료가 Land 진입을 발생시켰는지 나타냅니다 */
	bool bReachedLand = false;

	/** Downed 활성화가 Knockdown Montage를 정상적으로 교체하는 중인지 나타냅니다 */
	bool bIsTransitioningToDowned = false;

	/** Land 구간에 적용한 빠른 기상 가용 상태를 정확히 회수하기 위한 핸들입니다 */
	FActiveGameplayEffectHandle QuickGetUpAvailabilityEffectHandle;
};
