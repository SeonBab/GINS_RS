// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RSBaseGameplayAbility.h"
#include "RSGameplayAbility_Sprint.generated.h"

class UCharacterMovementComponent;

/** 입력을 유지하는 동안 캐릭터의 최대 이동 속도를 높이는 스프린트 어빌리티입니다 */
UCLASS(Abstract, Blueprintable)
class RS_API URSGameplayAbility_Sprint : public URSBaseGameplayAbility
{
	GENERATED_BODY()

public:
	URSGameplayAbility_Sprint();

protected:
	/** 스프린트에 필요한 객체를 확인하고 이동 속도와 상태 태그를 적용합니다 */
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	/** 어빌리티가 종료되거나 취소될 때 기존 이동 속도와 상태 태그를 복구합니다 */
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	/** 스프린트 입력이 해제되면 실행 중인 어빌리티를 정상 종료합니다 */
	UFUNCTION()
	void HandleInputReleased(float TimeHeld);

private:
	/** GA_Sprint의 Class Defaults에서 설정할 스프린트 최대 이동 속도입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sprint", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm/s"))
	float SprintSpeed = 900.0f;

	/** Sprint 종료 시 복구할 기존 최대 이동 속도입니다 */
	float CachedMaxWalkSpeed = 0.0f;

	/** Sprint 설정이 실제로 적용됐는지 나타냅니다 */
	bool bSprintApplied = false;

	/** Sprint 종료 시 속도를 복구할 이동 컴포넌트를 안전하게 참조합니다 */
	TWeakObjectPtr<UCharacterMovementComponent> CachedMovementComponent;
};
