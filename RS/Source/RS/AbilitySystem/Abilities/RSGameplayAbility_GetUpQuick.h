// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "RSBaseGameplayAbility_GetUp.h"
#include "RSGameplayAbility_GetUpQuick.generated.h"

class URSAbilityTask_CurveMovement;

/**
 * 누운 상태에서 입력을 받아 구르며 일어나는 빠른 기상입니다
 * 평상시에는 누운 상태가 아니라 활성화되지 않으므로 같은 입력 태그를 대시와 공유할 수 있습니다
 */
UCLASS(Abstract, Blueprintable)
class RS_API URSGameplayAbility_GetUpQuick : public URSBaseGameplayAbility_GetUp
{
	GENERATED_BODY()

public:
	URSGameplayAbility_GetUpQuick();

protected:
	/** 커서 방향으로 캐릭터를 돌리고 구르기 이동을 시작합니다 */
	virtual void StartGetUpMovement(ACharacter& Character) override;

	/** 남은 구르기 이동을 정리합니다 */
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** 한 번의 구르기가 장애물에 막히지 않았을 때 이동할 전체 거리입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Get Up|Quick", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm"))
	float RollDistance = 400.0f;

	/** 구르기 이동을 진행할 시간이며 Montage 길이와 맞춥니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Get Up|Quick", meta = (ClampMin = "0.01", UIMin = "0.01", ForceUnits = "s"))
	float RollDuration = 0.5f;

	/** 정규화 시간 0~1을 전체 구르기 거리의 누적 진행률 0~1로 변환합니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Get Up|Quick")
	FRuntimeFloatCurve DistanceProgressCurve;

private:
	/** 현재 실행에서 구르기 이동을 담당하는 AbilityTask입니다 */
	UPROPERTY(Transient)
	TObjectPtr<URSAbilityTask_CurveMovement> ActiveRollTask;
};
