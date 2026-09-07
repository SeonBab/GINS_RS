// Fill out your copyright notice in the Description page of Project Settings.


#include "RSHealthSet.h"

#include "AbilitySystemComponent.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectExtension.h"
#include "RSAbilitySystemComponent.h"
#include "RSGameplayTags.h"

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

		// 방어 행동의 종류를 알지 않고 대상이 공개한 대미지 면역 계약만 확인합니다
		if (Data.Target.HasMatchingGameplayTag(RSGameplayTags::State_Immunity_Damage))
		{
			return;
		}

		if (LocalDamage <= 0.0f)
		{
			return;
		}

		const float OldHealth = GetHealth();

		// SetHealth는 Attribute 변경 델리게이트를 동기적으로 발생시키므로 사망 처리와 Encounter 종료가
		// 아래 통지보다 먼저 실행될 수 있습니다
		// 그 과정에서 AvatarActor가 사용 불가 상태가 되어도 처치 일격의 표시 위치를 잃지 않도록 좌표를 미리 캡처합니다
		FVector TargetLocation = FVector::ZeroVector;
		bool bHasTargetLocation = false;
		if (const AActor* TargetAvatarActor = Data.Target.GetAvatarActor())
		{
			// 월드 원점도 유효한 앵커이므로 좌표값이 아니라 별도 플래그로 캡처 성공을 판정합니다
			TargetLocation = TargetAvatarActor->GetActorLocation();
			bHasTargetLocation = true;
		}

		SetHealth(FMath::Clamp(OldHealth - LocalDamage, 0.0f, GetMaxHealth()));

		// 요청한 피해량이 아니라 Clamp를 거쳐 실제로 줄어든 값이 외부 시스템이 관찰해야 하는 결과입니다
		const float AppliedDamage = OldHealth - GetHealth();
		if (AppliedDamage <= 0.0f || !bHasTargetLocation)
		{
			return;
		}

		// 전투 피드백은 피해 체인을 시작한 주체에 귀속하므로 즉시 instigator가 아니라 최초 instigator를 사용합니다
		// 통지에 실패해도 위의 피해 적용은 이미 끝났으므로 표시 실패가 게임플레이 결과를 바꾸지 않습니다
		if (URSAbilitySystemComponent* InstigatorAbilitySystemComp = Cast<URSAbilitySystemComponent>(Data.EffectSpec.GetContext().GetOriginalInstigatorAbilitySystemComponent()))
		{
			InstigatorAbilitySystemComp->NotifyDamageDealt(AppliedDamage, TargetLocation);
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
