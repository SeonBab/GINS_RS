// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Combat/RSCombatFunctionLibrary.h"
#include "RSBaseGameplayAbility.h"
#include "RSBaseGameplayAbility_Attack.generated.h"

class UAnimMontage;
class UGameplayEffect;

/**
 * Montage를 재생하고 타임라인의 판정 시점마다 타격을 실행해 피해를 적용하는 공격 어빌리티의 기반입니다
 * 노리는 진영과 판정 범위를 데이터로 받으므로 플레이어와 적이 같은 클래스를 사용합니다
 */
UCLASS(Abstract, Blueprintable)
class RS_API URSBaseGameplayAbility_Attack : public URSBaseGameplayAbility
{
	GENERATED_BODY()

public:
	URSBaseGameplayAbility_Attack();

protected:
	/** 실행 조건을 확인하고 쿨다운을 적용한 뒤 공격 Montage와 판정 대기를 시작합니다 */
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	/** 중단된 Montage가 남긴 애니메이션 Gameplay State를 정리합니다 */
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

#if WITH_EDITOR
	/** Montage의 판정 Notify 개수와 HitChecks의 개수가 어긋나지 않았는지 검사합니다 */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	/**
	 * 이번 활성화의 타격 순서를 초기화하고 Montage와 판정 대기 Task를 시작합니다
	 * 파생 클래스가 Commit과 방향 결정을 마친 뒤 호출합니다
	 */
	void StartAttackMontage();

	/** 공격 Montage가 정상 완료되면 어빌리티를 종료합니다 */
	UFUNCTION()
	virtual void HandleAttackMontageCompleted();

	/** 공격 Montage가 중단되면 취소 종료합니다 */
	UFUNCTION()
	virtual void HandleAttackMontageInterrupted();

	/** 공격 Montage Task가 취소되면 취소 종료합니다 */
	UFUNCTION()
	virtual void HandleAttackMontageCancelled();

	/**
	 * Montage의 판정 시점에 이번 타격의 정의를 소비하고 판정과 피해 적용을 실행합니다
	 * Notify는 타격을 구분하지 않으므로 HitChecks를 타임라인 순서대로 소비합니다
	 */
	UFUNCTION()
	void HandleHitCheckEvent(FGameplayEventData Payload);

	/** 판정에 걸린 대상 하나에게 공용 대미지 GameplayEffect를 적용합니다 */
	void ApplyDamageToTarget(AActor* TargetActor, float DamageAmount);

	/**
	 * 판정에 걸린 대상 하나에게 이번 타격이 요청하는 피격 반응을 전달합니다
	 * 요청을 실제로 적용할지는 대상의 면역 태그와 반응 Ability가 결정합니다
	 */
	void SendHitReactionToTarget(AActor* TargetActor, const FRSHitCheckDefinition& HitCheck) const;

protected:
	/** 이 공격이 재생할 Montage입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Attack")
	TObjectPtr<UAnimMontage> AttackMontage;

	/** 이 공격이 실행할 타격 목록이며 Montage 타임라인의 판정 Notify 순서와 같은 순서로 나열합니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Attack|Hit")
	TArray<FRSHitCheckDefinition> HitChecks;

	/** 이 공격이 노릴 진영의 허트박스 Trace Channel입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Attack|Hit")
	TEnumAsByte<ECollisionChannel> TargetChannel = ECollisionChannel::ECC_GameTraceChannel2;

	/** 판정에 걸린 대상에게 적용할 대미지 GameplayEffect입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Attack|Hit")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

private:
	/** 다음 판정에서 소비할 HitChecks의 인덱스이며 활성화마다 초기화합니다 */
	UPROPERTY(Transient)
	int32 NextHitCheckIndex = 0;
};
