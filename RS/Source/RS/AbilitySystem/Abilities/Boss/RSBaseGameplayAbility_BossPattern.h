// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RSBaseGameplayAbility.h"
#include "RSBaseGameplayAbility_BossPattern.generated.h"

/**
 * 보스가 페이즈에서 사용하는 공격 패턴의 공통 부모입니다
 * 어느 패턴이든 페이즈의 메인 기믹으로 지정될 수 있도록 파훼 판정과 결과 보고를 담당합니다
 * 이번 실행이 실제로 기믹인지는 페이즈 상태를 아는 URSBossPhaseComponent가 정하므로 패턴은 알지 않습니다
 */
UCLASS(Abstract)
class RS_API URSBaseGameplayAbility_BossPattern : public URSBaseGameplayAbility
{
	GENERATED_BODY()

public:
	/**
	 * 이 패턴에 파훼 판정이 있는지 반환합니다
	 * 판정이 없는 패턴을 메인 기믹으로 지정하면 그 페이즈가 로그 없이 항상 실패하므로 Data Validation이 이 값을 검사합니다
	 */
	virtual bool HasGimmickBreakCondition() const { return true; }

protected:
	/** 같은 인스턴스를 재사용하므로 이전 실행의 피격 기록이 남지 않게 시작할 때 초기화합니다 */
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	/** 파훼 판정 결과를 페이즈 컴포넌트에 보고합니다 */
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/**
	 * 판정에 걸린 대상을 기록한 뒤 피해를 적용합니다
	 * 패턴마다 판정 방식이 달라도 피해 적용은 모두 이 지점을 지나므로 여기서 한 번만 셉니다
	 */
	virtual void ApplyDamageToTarget(AActor* TargetActor, TSubclassOf<UGameplayEffect> DamageEffectClass, float DamageAmount) override;

	/**
	 * 이번 실행에서 판정에 걸린 참가자가 한 명도 없으면 파훼로 봅니다
	 * 피격 여부로 판정할 수 없는 패턴은 이 함수를 재정의해 자신의 조건을 사용합니다
	 */
	virtual bool IsGimmickBroken() const { return !bAnyTargetHit; }

private:
	/** 이번 실행에서 판정에 걸린 대상이 있었는지 나타냅니다 */
	bool bAnyTargetHit = false;
};
