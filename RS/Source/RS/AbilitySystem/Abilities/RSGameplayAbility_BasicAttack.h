// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RSBaseGameplayAbility_Attack.h"
#include "RSGameplayAbility_BasicAttack.generated.h"

class UGameplayEffect;

/**
 * 기본 공격 한 단계의 방향 전환과 콤보 준비 상태를 관리하는 어빌리티입니다
 * 인스턴스 하나가 기본 공격 전체가 아니라 콤보의 한 타에 대응하며, 단계는 에셋으로 나눕니다
 * Montage 수명과 타격 판정은 기반 클래스가 소유합니다
 */
UCLASS(Abstract, Blueprintable)
class RS_API URSGameplayAbility_BasicAttack : public URSBaseGameplayAbility_Attack
{
	GENERATED_BODY()

public:
	URSGameplayAbility_BasicAttack();

protected:
	/** 커서 방향으로 캐릭터를 돌리고 현재 단계의 Montage 수명을 구성합니다 */
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	/** 공격 Montage가 정상 완료되면 다음 콤보 상태를 적용하고 종료합니다 */
	virtual void HandleAttackMontageCompleted() override;

	/** Activation Required Tags에서 현재 단계를 연 콤보 준비 상태를 반환합니다 */
	FGameplayTag GetRequiredComboReadyTag() const;

	/** 현재 단계를 열었던 준비 상태 GameplayEffect를 소비합니다 */
	void ConsumeRequiredComboState();

	/** 다음 단계가 있다면 제한시간을 가진 준비 상태 GameplayEffect를 적용합니다 */
	void ApplyNextComboState();

protected:
	/** 현재 단계가 정상 완료되면 부여할 다음 준비 태그입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Basic Attack|Combo", meta = (Categories = "State.Combo.BasicAttack.Ready"))
	FGameplayTag NextComboReadyTag;

	/** 다음 공격 입력을 기다릴 시간입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Basic Attack|Combo", meta = (ClampMin = "0.01", UIMin = "0.01", ForceUnits = "s"))
	float ComboReadyDuration = 1.0f;

	/** 다음 준비 태그와 제한시간을 적용할 GameplayEffect입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Basic Attack|Combo")
	TSubclassOf<UGameplayEffect> ComboStateEffectClass;
};
