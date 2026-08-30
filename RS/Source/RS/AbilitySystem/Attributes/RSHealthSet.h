// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RSAttributeSet.h"
#include "RSHealthSet.generated.h"

struct FGameplayEffectModCallbackData;

/** RS 캐릭터의 체력 관련 Attribute를 정의할 AttributeSet입니다 */
UCLASS()
class RS_API URSHealthSet : public URSAttributeSet
{
	GENERATED_BODY()

public:
	URSHealthSet();

	/** 현재 체력 Attribute의 표준 접근자를 제공합니다 */
	RS_ATTRIBUTE_ACCESSORS(URSHealthSet, Health);

	/** 최대 체력 Attribute의 표준 접근자를 제공합니다 */
	RS_ATTRIBUTE_ACCESSORS(URSHealthSet, MaxHealth);

	/** 적용된 회복량 Attribute의 표준 접근자를 제공합니다 */
	RS_ATTRIBUTE_ACCESSORS(URSHealthSet, Healing);

	/** 적용된 피해량 Attribute의 표준 접근자를 제공합니다 */
	RS_ATTRIBUTE_ACCESSORS(URSHealthSet, Damage);

protected:
	/** Attribute가 변경되기 전에 유효한 범위로 제한합니다 */
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	/** GameplayEffect 결과를 실제 Health에 반영합니다 */
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

private:
	/**
	 * 현재 체력입니다
	 * HideFromModifiers로 일반 GameplayEffect가 직접 수정하지 못하게 합니다
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Health", meta = (AllowPrivateAccess = "true", HideFromModifiers))
	FGameplayAttributeData Health;

	/** 현재 체력이 가질 수 있는 최대값입니다 */
	UPROPERTY(BlueprintReadOnly, Category = "Health", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData MaxHealth;

	/**
	 * 이번 GameplayEffect에서 적용된 회복량입니다
	 * Health에 반영한 뒤 즉시 0으로 초기화합니다
	 */
	UPROPERTY(BlueprintReadOnly,Category = "Health", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData Healing;

	/**
	 * 이번 GameplayEffect에서 적용된 피해량입니다
	 * Health에 반영한 뒤 즉시 0으로 초기화합니다
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Health", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData Damage;
};
