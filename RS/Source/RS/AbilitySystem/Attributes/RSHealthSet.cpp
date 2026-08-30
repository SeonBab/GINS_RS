// Fill out your copyright notice in the Description page of Project Settings.


#include "RSHealthSet.h"

#include "GameplayEffectExtension.h"

URSHealthSet::URSHealthSet() : Health(100.0f), MaxHealth(100.0f), Healing(0.0f), Damage(0.0f)
{

}

void URSHealthSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetMaxHealthAttribute())
	{
		// 최대 체력이 0 이하가 되지 않도록 제한합니다
		NewValue = FMath::Max(NewValue, 1.0f);
	}
	else if (Attribute == GetHealthAttribute())
	{
		// 현재 체력을 0과 최대 체력 사이로 제한합니다
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
	}
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
	else if (Data.EvaluatedData.Attribute == GetMaxHealthAttribute())
	{
		// 최대 체력이 감소하면 현재 체력도 새로운 최대값으로 제한합니다
		SetMaxHealth(FMath::Max(GetMaxHealth(), 1.0f));

		SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));
	}
}
