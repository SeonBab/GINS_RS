// Fill out your copyright notice in the Description page of Project Settings.


#include "RSHealthComponent.h"

#include "AbilitySystemComponent.h"
#include "RSAbilitySystemComponent.h"
#include "RSHealthSet.h"

URSHealthComponent::URSHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void URSHealthComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UninitializeFromAbilitySystem();

	Super::EndPlay(EndPlayReason);
}

void URSHealthComponent::InitializeWithAbilitySystem(URSAbilitySystemComponent* InAbilitySystemComponent)
{
	if (!InAbilitySystemComponent)
	{
		return;
	}

	if (AbilitySystemComp == InAbilitySystemComponent && HealthSet)
	{
		return;
	}

	// 다른 ASC에 연결되어 있었다면 기존 델리게이트부터 정리합니다
	UninitializeFromAbilitySystem();

	const URSHealthSet* FoundHealthSet = InAbilitySystemComponent->GetSet<URSHealthSet>();

	if (!FoundHealthSet)
	{
		return;
	}

	AbilitySystemComp = InAbilitySystemComponent;
	HealthSet = FoundHealthSet;

	// ASC가 제공하는 Attribute 변경 델리게이트를 컴포넌트의 외부 이벤트로 연결합니다
	HealthChangedDelegateHandle = AbilitySystemComp->GetGameplayAttributeValueChangeDelegate(URSHealthSet::GetHealthAttribute()).AddUObject(this, &ThisClass::HandleHealthChanged);

	MaxHealthChangedDelegateHandle = AbilitySystemComp->GetGameplayAttributeValueChangeDelegate(URSHealthSet::GetMaxHealthAttribute()).AddUObject(this, &ThisClass::HandleMaxHealthChanged);

	// 초기화 시점에도 현재 값을 전달하여 먼저 바인딩된 시스템이 초기 상태를 동기화할 수 있게 합니다
	OnHealthChanged.Broadcast(this, GetHealth(), GetHealth());
	OnMaxHealthChanged.Broadcast(this, GetMaxHealth(), GetMaxHealth());
}

void URSHealthComponent::UninitializeFromAbilitySystem()
{
	if (AbilitySystemComp)
	{
		if (HealthChangedDelegateHandle.IsValid())
		{
			AbilitySystemComp
				->GetGameplayAttributeValueChangeDelegate(
					URSHealthSet::GetHealthAttribute())
				.Remove(HealthChangedDelegateHandle);
		}

		if (MaxHealthChangedDelegateHandle.IsValid())
		{
			AbilitySystemComp
				->GetGameplayAttributeValueChangeDelegate(
					URSHealthSet::GetMaxHealthAttribute())
				.Remove(MaxHealthChangedDelegateHandle);
		}
	}

	HealthChangedDelegateHandle.Reset();
	MaxHealthChangedDelegateHandle.Reset();

	HealthSet = nullptr;
	AbilitySystemComp = nullptr;
}

float URSHealthComponent::GetHealth() const
{
	return HealthSet ? HealthSet->GetHealth() : 0.0f;
}

float URSHealthComponent::GetMaxHealth() const
{
	return HealthSet ? HealthSet->GetMaxHealth() : 0.0f;
}

float URSHealthComponent::GetHealthNormalized() const
{
	const float MaxHealth = GetMaxHealth();

	// 초기화 전이거나 최대 체력이 유효하지 않으면 안전하게 0을 반환합니다
	return MaxHealth > 0.0f ? FMath::Clamp(GetHealth() / MaxHealth, 0.0f, 1.0f) : 0.0f;
}

void URSHealthComponent::HandleHealthChanged(const FOnAttributeChangeData& ChangeData)
{
	OnHealthChanged.Broadcast(this, ChangeData.OldValue, ChangeData.NewValue);
}

void URSHealthComponent::HandleMaxHealthChanged(const FOnAttributeChangeData& ChangeData)
{
	OnMaxHealthChanged.Broadcast(this, ChangeData.OldValue, ChangeData.NewValue);
}
