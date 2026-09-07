// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RSBaseGameplayAbility.h"
#include "RSGameplayAbility_Skill.generated.h"

class UAnimMontage;

/** 몽타주 수명과 공통 쿨다운을 사용하는 일반 스킬 어빌리티입니다 */
UCLASS(Abstract, Blueprintable)
class RS_API URSGameplayAbility_Skill : public URSBaseGameplayAbility
{
	GENERATED_BODY()

public:
	URSGameplayAbility_Skill();

protected:
	/** 필수 설정을 검증하고 쿨다운을 적용한 뒤 스킬 Montage를 재생합니다 */
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	/** 중단된 Montage가 남긴 애니메이션 Gameplay State를 정리합니다 */
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	/** 스킬 Montage가 정상 완료되면 현재 어빌리티를 종료합니다 */
	UFUNCTION()
	void HandleSkillMontageCompleted();

	/** 스킬 Montage가 중단되거나 취소되면 현재 어빌리티를 취소 종료합니다 */
	UFUNCTION()
	void HandleSkillMontageCancelled();

protected:
	/** 스킬의 전체 수명과 시각적 동작을 결정하는 Montage입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Skill")
	TObjectPtr<UAnimMontage> SkillMontage;

	/** 스킬 Montage의 재생 속도입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Skill", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float SkillMontagePlayRate = 1.0f;
};
