// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RSBaseGameplayAbility.h"
#include "RSGameplayAbility_BasicAttack.generated.h"

/** 현재 Navigation 이동을 중단하고 마우스 커서 방향으로 캐릭터를 회전시키는 기본 공격 어빌리티입니다 */
UCLASS(Blueprintable)
class RS_API URSGameplayAbility_BasicAttack : public URSBaseGameplayAbility
{
	GENERATED_BODY()

public:
	URSGameplayAbility_BasicAttack();

protected:
	/** 기본 공격을 시작하기 위한 이동 중단과 커서 방향 회전을 수행합니다 */
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

private:
	/** 임시 공격 간격이 끝나면 현재 공격을 정상 종료합니다 */
	UFUNCTION()
	void HandleTemporaryAttackIntervalFinished();

private:
	/**
	 * 공격 Montage가 준비되기 전 버튼 유지 반복을 검증하기 위한 임시 공격 간격입니다
	 * 추후 Montage 재생 시간이 Ability 수명을 소유하면 제거하거나 해당 시간 기준으로 대체합니다
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Basic Attack", meta = (AllowPrivateAccess = "true", ClampMin = "0.01", UIMin = "0.01", ForceUnits = "s"))
	float TemporaryAttackInterval = 0.5f;
};
