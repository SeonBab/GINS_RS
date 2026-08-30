// Fill out your copyright notice in the Description page of Project Settings.


#include "RSHealthSet.h"

#include "GameplayEffectExtension.h"

URSHealthSet::URSHealthSet()
	: Health(100.0f)
	, MaxHealth(100.0f)
	, Healing(0.0f)
	, Damage(0.0f)
{
	// 초기화 GameplayEffect가 적용되기 전에도 유효한 체력 상태를 유지합니다
}

void URSHealthSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);

	// 영구적인 Base Value 변경에도 HealthSet의 범위 규칙을 적용합니다
	ClampAttribute(Attribute, NewValue);
}

void URSHealthSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	// GameplayEffect Modifier가 반영된 Current Value에도 같은 규칙을 적용합니다
	ClampAttribute(Attribute, NewValue);
}

void URSHealthSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetDamageAttribute())
	{
		// 음수 피해는 허용하지 않습니다
		const float LocalDamage = FMath::Max(GetDamage(), 0.0f);

		// Damage는 일회성 전달 값이므로 처리 전에 초기화합니다
		SetDamage(0.0f);

		if (LocalDamage > 0.0f)
		{
			SetHealth(FMath::Clamp(GetHealth() - LocalDamage, 0.0f, GetMaxHealth()));
		}
	}
	else if (Data.EvaluatedData.Attribute == GetHealingAttribute())
	{
		// 음수 회복은 허용하지 않습니다
		const float LocalHealing = FMath::Max(GetHealing(), 0.0f);

		// Healing도 실제 체력에 반영한 뒤 남기지 않습니다
		SetHealing(0.0f);

		if (LocalHealing > 0.0f)
		{
			SetHealth(FMath::Clamp(GetHealth() + LocalHealing, 0.0f, GetMaxHealth()));
		}
	}
	else if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		// C++ Execution 등으로 Health가 직접 변경된 경우에도 최종 범위를 보정합니다
		SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));
	}
	else if (Data.EvaluatedData.Attribute == GetMaxHealthAttribute())
	{
		// 최대 체력이 감소하면 현재 체력도 새로운 최대값으로 제한합니다
		SetMaxHealth(FMath::Max(GetMaxHealth(), 1.0f));

		SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));
	}
}

void URSHealthSet::ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const
{
	if (Attribute == GetHealthAttribute())
	{
		// 현재 체력은 0보다 작거나 최대 체력보다 클 수 없습니다
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
	}
	else if (Attribute == GetMaxHealthAttribute())
	{
		// 최대 체력은 Health의 유효 범위를 유지할 수 있도록 최소 1을 보장합니다
		NewValue = FMath::Max(NewValue, 1.0f);
	}
}
