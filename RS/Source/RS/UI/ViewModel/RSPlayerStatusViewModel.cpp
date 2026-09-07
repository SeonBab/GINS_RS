// Fill out your copyright notice in the Description page of Project Settings.

#include "RSPlayerStatusViewModel.h"

#include "RSHealthComponent.h"
#include "RSPlayerCharacter.h"

void URSPlayerStatusViewModel::BeginDestroy()
{
	DisconnectHealthComponent();

	Super::BeginDestroy();
}

void URSPlayerStatusViewModel::HandleSourceRegistered(UObject* Source)
{
	ARSPlayerCharacter* PlayerCharacter = Cast<ARSPlayerCharacter>(Source);
	if (!IsValid(PlayerCharacter))
	{
		return;
	}

	InitializeViewModel(PlayerCharacter->GetHealthComponent());
}

void URSPlayerStatusViewModel::HandleSourceUnregistered(UObject* Source)
{
	const ARSPlayerCharacter* PlayerCharacter = Cast<ARSPlayerCharacter>(Source);
	if (!PlayerCharacter || HealthComponent.Get() != PlayerCharacter->GetHealthComponent())
	{
		return;
	}

	UninitializeViewModel();
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

	ApplyHealthValues(NewValue, InHealthComponent->GetMaxHealth(), InHealthComponent->GetHealthNormalized());
}

void URSPlayerStatusViewModel::HandleMaxHealthChanged(URSHealthComponent* InHealthComponent, float, float NewValue)
{
	if (HealthComponent.Get() != InHealthComponent)
	{
		return;
	}

	ApplyHealthValues(InHealthComponent->GetHealth(), NewValue, InHealthComponent->GetHealthNormalized());
}

void URSPlayerStatusViewModel::UpdateHealthValues()
{
	URSHealthComponent* CurrentHealthComponent = HealthComponent.Get();
	if (!IsValid(CurrentHealthComponent))
	{
		ResetHealthValues();
		return;
	}

	ApplyHealthValues(CurrentHealthComponent->GetHealth(), CurrentHealthComponent->GetMaxHealth(), CurrentHealthComponent->GetHealthNormalized());
}

void URSPlayerStatusViewModel::ResetHealthValues()
{
	UE_MVVM_SET_PROPERTY_VALUE(Health, 0.0f);
	UE_MVVM_SET_PROPERTY_VALUE(MaxHealth, 0.0f);
	UE_MVVM_SET_PROPERTY_VALUE(HealthNormalized, 0.0f);

	// 데이터 원본이 없는 상태를 0 / 0으로 표시하면 사망과 구분되지 않으므로 숫자를 감춥니다
	UE_MVVM_SET_PROPERTY_VALUE(HealthText, FText::GetEmpty());
}

void URSPlayerStatusViewModel::ApplyHealthValues(float InHealth, float InMaxHealth, float InHealthNormalized)
{
	UE_MVVM_SET_PROPERTY_VALUE(Health, InHealth);
	UE_MVVM_SET_PROPERTY_VALUE(MaxHealth, InMaxHealth);
	UE_MVVM_SET_PROPERTY_VALUE(HealthNormalized, InHealthNormalized);
	UE_MVVM_SET_PROPERTY_VALUE(HealthText, MakeHealthText(InHealth, InMaxHealth));
}

FText URSPlayerStatusViewModel::MakeHealthText(float InHealth, float InMaxHealth)
{
	// 체력이 1보다 적게 남은 상태가 0으로 보여 사망으로 오해되지 않도록 올림으로 표시합니다
	const int32 DisplayHealth = FMath::CeilToInt(FMath::Max(InHealth, 0.0f));
	const int32 DisplayMaxHealth = FMath::CeilToInt(FMath::Max(InMaxHealth, 0.0f));

	return FText::AsCultureInvariant(FString::Printf(TEXT("%d / %d"), DisplayHealth, DisplayMaxHealth));
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
