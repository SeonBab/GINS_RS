// Fill out your copyright notice in the Description page of Project Settings.

#include "RSBossStatusViewModel.h"

#include "RSBossCharacter.h"
#include "RSHealthComponent.h"

void URSBossStatusViewModel::BeginDestroy()
{
	DisconnectHealthComponent();

	Super::BeginDestroy();
}

void URSBossStatusViewModel::HandleSourceRegistered(UObject* Source)
{
	ARSBossCharacter* BossCharacter = Cast<ARSBossCharacter>(Source);
	if (!IsValid(BossCharacter))
	{
		return;
	}

	InitializeViewModel(BossCharacter->GetHealthComponent(), BossCharacter->GetBossName());
}

void URSBossStatusViewModel::HandleSourceUnregistered(UObject* Source)
{
	const ARSBossCharacter* BossCharacter = Cast<ARSBossCharacter>(Source);
	if (!BossCharacter || HealthComponent.Get() != BossCharacter->GetHealthComponent())
	{
		return;
	}

	UninitializeViewModel();
}

void URSBossStatusViewModel::InitializeViewModel(URSHealthComponent* InHealthComponent, const FText& InBossName)
{
	if (IsValid(InHealthComponent) && HealthComponent.Get() == InHealthComponent)
	{
		UE_MVVM_SET_PROPERTY_VALUE(BossName, InBossName);
		UpdateHealthValues();
		UE_MVVM_SET_PROPERTY_VALUE(bIsVisible, true);
		return;
	}

	DisconnectHealthComponent();

	if (!IsValid(InHealthComponent))
	{
		ResetStatusValues();
		return;
	}

	HealthComponent = InHealthComponent;
	UE_MVVM_SET_PROPERTY_VALUE(BossName, InBossName);
	InHealthComponent->OnHealthChanged.AddUniqueDynamic(this, &ThisClass::HandleHealthChanged);
	InHealthComponent->OnMaxHealthChanged.AddUniqueDynamic(this, &ThisClass::HandleMaxHealthChanged);

	// 이벤트를 구독하기 전에 변경된 값도 반영할 수 있도록 현재 상태를 즉시 동기화합니다
	UpdateHealthValues();
	UE_MVVM_SET_PROPERTY_VALUE(bIsVisible, true);
}

void URSBossStatusViewModel::UninitializeViewModel()
{
	DisconnectHealthComponent();
	ResetStatusValues();
}

void URSBossStatusViewModel::HandleHealthChanged(URSHealthComponent* InHealthComponent, float, float NewValue)
{
	if (HealthComponent.Get() != InHealthComponent)
	{
		return;
	}

	UE_MVVM_SET_PROPERTY_VALUE(Health, NewValue);
	UE_MVVM_SET_PROPERTY_VALUE(MaxHealth, InHealthComponent->GetMaxHealth());
	UE_MVVM_SET_PROPERTY_VALUE(HealthNormalized, InHealthComponent->GetHealthNormalized());
}

void URSBossStatusViewModel::HandleMaxHealthChanged(URSHealthComponent* InHealthComponent, float, float NewValue)
{
	if (HealthComponent.Get() != InHealthComponent)
	{
		return;
	}

	UE_MVVM_SET_PROPERTY_VALUE(Health, InHealthComponent->GetHealth());
	UE_MVVM_SET_PROPERTY_VALUE(MaxHealth, NewValue);
	UE_MVVM_SET_PROPERTY_VALUE(HealthNormalized, InHealthComponent->GetHealthNormalized());
}

void URSBossStatusViewModel::UpdateHealthValues()
{
	URSHealthComponent* CurrentHealthComponent = HealthComponent.Get();
	if (!IsValid(CurrentHealthComponent))
	{
		ResetStatusValues();
		return;
	}

	UE_MVVM_SET_PROPERTY_VALUE(Health, CurrentHealthComponent->GetHealth());
	UE_MVVM_SET_PROPERTY_VALUE(MaxHealth, CurrentHealthComponent->GetMaxHealth());
	UE_MVVM_SET_PROPERTY_VALUE(HealthNormalized, CurrentHealthComponent->GetHealthNormalized());
}

void URSBossStatusViewModel::ResetStatusValues()
{
	UE_MVVM_SET_PROPERTY_VALUE(BossName, FText::GetEmpty());
	UE_MVVM_SET_PROPERTY_VALUE(Health, 0.0f);
	UE_MVVM_SET_PROPERTY_VALUE(MaxHealth, 0.0f);
	UE_MVVM_SET_PROPERTY_VALUE(HealthNormalized, 0.0f);
	UE_MVVM_SET_PROPERTY_VALUE(bIsVisible, false);
}

void URSBossStatusViewModel::DisconnectHealthComponent()
{
	if (URSHealthComponent* CurrentHealthComponent = HealthComponent.Get())
	{
		CurrentHealthComponent->OnHealthChanged.RemoveDynamic(this, &ThisClass::HandleHealthChanged);
		CurrentHealthComponent->OnMaxHealthChanged.RemoveDynamic(this, &ThisClass::HandleMaxHealthChanged);
	}

	HealthComponent.Reset();
}
