// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "RSBaseGameplayAbility.h"
#include "RSGameplayAbility_Dash.generated.h"

class ACharacter;
class UAnimMontage;
class URSAbilityTask_CurveMovement;

/** 마우스 커서 방향으로 거리 진행률 커브를 따라 이동하는 대시 어빌리티입니다 */
UCLASS(Abstract, Blueprintable)
class RS_API URSGameplayAbility_Dash : public URSBaseGameplayAbility
{
	GENERATED_BODY()

public:
	URSGameplayAbility_Dash();

protected:
	/** 필요한 실행 Context를 검증하고 쿨다운을 적용한 뒤 대시 이동을 시작합니다 */
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	/** 정상 종료와 외부 취소에서 현재 실행의 커브 이동과 애니메이션 상태를 정리합니다 */
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	/** 마우스 커서의 월드 위치를 우선하여 이번 실행에서 고정할 대시 방향을 반환합니다 */
	FVector GetDashDirection(const ACharacter* Character, const FGameplayAbilityActorInfo* ActorInfo) const;

	/** 대시 Montage 또는 fallback 지속 시간이 끝나면 현재 어빌리티를 정상 종료합니다 */
	UFUNCTION()
	void HandleDashFinished();

	/** 대시 Montage가 중단되거나 취소되면 현재 어빌리티를 취소 종료합니다 */
	UFUNCTION()
	void HandleDashCancelled();

private:
	/** 한 번의 대시가 장애물에 막히지 않았을 때 이동할 전체 거리입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Dash", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm"))
	float DashDistance = 600.0f;

	/** 대시의 전체 수명과 시각적 동작을 결정하는 Montage입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Dash", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimMontage> DashMontage;

	/** Montage 재생 속도와 커브 이동 시간을 함께 조정합니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Dash", meta = (AllowPrivateAccess = "true", ClampMin = "0.01", UIMin = "0.01"))
	float DashMontagePlayRate = 1.0f;

	/** Montage가 지정되지 않았을 때 거리 진행률 커브를 재생할 fallback 지속 시간입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Dash", meta = (AllowPrivateAccess = "true", ClampMin = "0.01", UIMin = "0.01", ForceUnits = "s"))
	float DashDuration = 0.45f;

	/** 정규화 시간 0~1을 전체 대시 거리의 누적 진행률 0~1로 변환합니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Dash", meta = (AllowPrivateAccess = "true"))
	FRuntimeFloatCurve DistanceProgressCurve;

	/** 현재 실행에서 누적 거리 진행률 커브 이동을 담당하는 AbilityTask입니다 */
	UPROPERTY(Transient)
	TObjectPtr<URSAbilityTask_CurveMovement> ActiveDashMovementTask;
};
