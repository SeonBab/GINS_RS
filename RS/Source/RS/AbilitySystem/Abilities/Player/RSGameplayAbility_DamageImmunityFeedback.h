// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RSBaseGameplayAbility.h"
#include "RSNiagaraSpawnDefinition.h"
#include "RSGameplayAbility_DamageImmunityFeedback.generated.h"

class USoundBase;

/**
 * 대미지 면역이 피해를 막았을 때 그 사실을 알리는 연출만 재생하는 어빌리티입니다
 * 무적을 만든 행동의 종류를 알지 않고 면역이 막았다는 통지만 받습니다
 */
UCLASS(Abstract, Blueprintable)
class RS_API URSGameplayAbility_DamageImmunityFeedback : public URSBaseGameplayAbility
{
	GENERATED_BODY()

public:
	URSGameplayAbility_DamageImmunityFeedback();

protected:
	/** 연출을 재생하고 활성 상태를 남기지 않고 종료합니다 */
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	/**
	 * 피해를 막은 자리에 재생할 Niagara입니다
	 * 소켓 기준으로 두면 캐릭터를 따라가고 World Transform으로 두면 Avatar 위치에 한 번 생성됩니다
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Damage Immunity Feedback")
	FRSNiagaraSpawnDefinition ImmunityNiagara;

	/** 피해를 막았을 때 Avatar 위치에서 재생할 사운드입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Damage Immunity Feedback")
	TObjectPtr<USoundBase> ImmunitySound;
};
