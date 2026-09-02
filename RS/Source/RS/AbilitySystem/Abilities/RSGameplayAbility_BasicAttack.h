// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RSBaseGameplayAbility.h"
#include "RSGameplayAbility_BasicAttack.generated.h"

class UAnimMontage;
class UGameplayEffect;

/** 기본 공격 한 단계의 방향 전환, Montage 수명과 콤보 준비 상태를 관리하는 어빌리티입니다 */
UCLASS(Abstract, Blueprintable)
class RS_API URSGameplayAbility_BasicAttack : public URSBaseGameplayAbility
{
	GENERATED_BODY()

public:
	URSGameplayAbility_BasicAttack();

protected:
	/** 기본 공격을 시작하고 현재 단계의 Montage 수명을 구성합니다 */
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	/** 중단된 Montage가 남긴 애니메이션 Gameplay State를 정리합니다 */
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/** 공격 Montage가 정상 완료되면 다음 콤보 상태를 적용하고 종료합니다 */
	UFUNCTION()
	void HandleAttackMontageCompleted();

	/** 공격 Montage가 중단되면 다음 콤보 상태 없이 취소 종료합니다 */
	UFUNCTION()
	void HandleAttackMontageInterrupted();

	/** 공격 Montage Task가 취소되면 현재 Ability를 취소 종료합니다 */
	UFUNCTION()
	void HandleAttackMontageCancelled();

	/** Activation Required Tags에서 현재 단계를 연 콤보 준비 상태를 반환합니다 */
	FGameplayTag GetRequiredComboReadyTag() const;

	/** 현재 단계를 열었던 준비 상태 GameplayEffect를 소비합니다 */
	void ConsumeRequiredComboState();

	/** 다음 단계가 있다면 제한시간을 가진 준비 상태 GameplayEffect를 적용합니다 */
	void ApplyNextComboState();

protected:
	/** 현재 공격 단계에서 재생할 Montage입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Basic Attack", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimMontage> AttackMontage;

	/** 현재 단계가 정상 완료되면 부여할 다음 준비 태그입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Basic Attack|Combo", meta = (AllowPrivateAccess = "true", Categories = "State.Combo.BasicAttack.Ready"))
	FGameplayTag NextComboReadyTag;

	/** 다음 공격 입력을 기다릴 시간입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Basic Attack|Combo", meta = (AllowPrivateAccess = "true", ClampMin = "0.01", UIMin = "0.01", ForceUnits = "s"))
	float ComboReadyDuration = 1.0f;

	/** 다음 준비 태그와 제한시간을 적용할 GameplayEffect입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Basic Attack|Combo", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGameplayEffect> ComboStateEffectClass;
};
