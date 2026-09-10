// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RSBaseGameplayAbility.h"
#include "RSBaseGameplayAbility_MainGimmick.generated.h"

class UGameplayEffect;

/**
 * 페이즈 전환 전에 실행하는 메인 기믹의 공통 수명을 담당합니다
 * 파훼 판정 자체는 파생 클래스가 하고, 이 클래스는 결과 보고와 실패 처리만 맡습니다
 */
UCLASS(Abstract)
class RS_API URSBaseGameplayAbility_MainGimmick : public URSBaseGameplayAbility
{
	GENERATED_BODY()

public:
	URSBaseGameplayAbility_MainGimmick();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	/** 종료 경로와 관계없이 판정 결과를 보고하고 실패한 경우 참가자에게 치명 피해를 적용합니다 */
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/**
	 * 플레이어가 기믹을 파훼했음을 기록합니다
	 * 파생 클래스가 파훼 조건을 만족한 시점에 한 번 호출하며, 호출하지 않으면 실패로 종료합니다
	 */
	void MarkGimmickBroken();

	/** 현재까지 파훼가 기록되었는지 반환합니다 */
	bool IsGimmickBroken() const { return bGimmickBroken; }

private:
	/** 파훼하지 못했을 때 Encounter의 모든 활성 참가자에게 치명 피해를 적용합니다 */
	void ApplyFailureDamageToParticipants();

protected:
	/** 파훼 실패 시 사용할 공용 대미지 GameplayEffect입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Gimmick")
	TSubclassOf<UGameplayEffect> FailureDamageEffectClass;

	/**
	 * 파훼 실패 시 참가자에게 적용할 피해량입니다
	 * 즉사를 별도 경로로 두지 않고 최대 체력을 넘는 값을 적용해 기존 사망 흐름을 그대로 사용합니다
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Gimmick", meta = (ClampMin = "0.0"))
	float FailureDamage = 99999.0f;

private:
	/** 이번 실행에서 파훼가 기록되었는지 나타냅니다 */
	bool bGimmickBroken = false;
};
