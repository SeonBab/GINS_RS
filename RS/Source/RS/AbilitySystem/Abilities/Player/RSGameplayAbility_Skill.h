// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RSBaseGameplayAbility_Attack.h"
#include "RSGameplayAbility_Skill.generated.h"

class UAnimMontage;

/** 기존 Montage 설정을 유지하면서 타격별 판정과 공통 쿨다운을 사용하는 일반 스킬 어빌리티입니다 */
UCLASS(Abstract, Blueprintable)
class RS_API URSGameplayAbility_Skill : public URSBaseGameplayAbility_Attack
{
	GENERATED_BODY()

public:
	URSGameplayAbility_Skill();

protected:
	/** 기존 스킬 Montage 설정을 공용 공격 판정 흐름에 제공합니다 */
	virtual UAnimMontage* GetAttackMontage() const override;

	/** 기존 스킬 Montage 재생 속도를 공용 공격 판정 흐름에 제공합니다 */
	virtual float GetAttackMontagePlayRate() const override;

protected:
	/** 스킬의 전체 수명과 시각적 동작을 결정하는 Montage입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Skill")
	TObjectPtr<UAnimMontage> SkillMontage;

	/** 스킬 Montage의 재생 속도입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Skill", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float SkillMontagePlayRate = 1.0f;
};
