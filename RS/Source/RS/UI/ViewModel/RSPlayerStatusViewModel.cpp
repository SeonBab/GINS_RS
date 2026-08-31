// Fill out your copyright notice in the Description page of Project Settings.

#include "RSPlayerStatusViewModel.h"

#include "RSHealthComponent.h"

void URSPlayerStatusViewModel::BeginDestroy()
{
	DisconnectHealthComponent();

	Super::BeginDestroy();
}

void URSPlayerStatusViewModel::InitializeViewModel(URSHealthComponent* InHealthComponent)
{
	if (IsValid(InHealthComponent) && HealthComponent.Get() == InHealthComponent)
	{
		UpdateHealthValues();
		return;
	}

	DisconnectHealthComponent();

	if (!IsValid(InHealthComponent))
	{
		ResetHealthValues();
		return;
	}

	HealthComponent = InHealthComponent;
	InHealthComponent->OnHealthChanged.AddUniqueDynamic(this, &ThisClass::HandleHealthChanged);
	InHealthComponent->OnMaxHealthChanged.AddUniqueDynamic(this, &ThisClass::HandleMaxHealthChanged);

	// 이벤트를 구독하기 전에 변경된 값도 반영할 수 있도록 현재 상태를 즉시 동기화합니다
	UpdateHealthValues();
}

void URSPlayerStatusViewModel::UninitializeViewModel()
{
	DisconnectHealthComponent();
	ResetHealthValues();
}

void URSPlayerStatusViewModel::HandleHealthChanged(URSHealthComponent* InHealthComponent, float, float NewValue)
{
	if (HealthComponent.Get() != InHealthComponent)
	{
		return;
	}

	UE_MVVM_SET_PROPERTY_VALUE(Health, NewValue);
	UE_MVVM_SET_PROPERTY_VALUE(MaxHealth, InHealthComponent->GetMaxHealth());
	UE_MVVM_SET_PROPERTY_VALUE(HealthNormalized, InHealthComponent->GetHealthNormalized());
}

void URSPlayerStatusViewModel::HandleMaxHealthChanged(URSHealthComponent* InHealthComponent, float, float NewValue)
{
	if (HealthComponent.Get() != InHealthComponent)
	{
		return;
	}

	UE_MVVM_SET_PROPERTY_VALUE(Health, InHealthComponent->GetHealth());
	UE_MVVM_SET_PROPERTY_VALUE(MaxHealth, NewValue);
	UE_MVVM_SET_PROPERTY_VALUE(HealthNormalized, InHealthComponent->GetHealthNormalized());
}

void URSPlayerStatusViewModel::UpdateHealthValues()
{
	URSHealthComponent* CurrentHealthComponent = HealthComponent.Get();
	if (!IsValid(CurrentHealthComponent))
	{
		ResetHealthValues();
		return;
	}

	UE_MVVM_SET_PROPERTY_VALUE(Health, CurrentHealthComponent->GetHealth());
	UE_MVVM_SET_PROPERTY_VALUE(MaxHealth, CurrentHealthComponent->GetMaxHealth());
	UE_MVVM_SET_PROPERTY_VALUE(HealthNormalized, CurrentHealthComponent->GetHealthNormalized());
}

void URSPlayerStatusViewModel::ResetHealthValues()
{
	UE_MVVM_SET_PROPERTY_VALUE(Health, 0.0f);
	UE_MVVM_SET_PROPERTY_VALUE(MaxHealth, 0.0f);
	UE_MVVM_SET_PROPERTY_VALUE(HealthNormalized, 0.0f);
}

void URSPlayerStatusViewModel::DisconnectHealthComponent()
{
	if (URSHealthComponent* CurrentHealthComponent = HealthComponent.Get())
	{
		CurrentHealthComponent->OnHealthChanged.RemoveDynamic(this, &ThisClass::HandleHealthChanged);
		CurrentHealthComponent->OnMaxHealthChanged.RemoveDynamic(this, &ThisClass::HandleMaxHealthChanged);
	}

	HealthComponent.Reset();
}
