// Fill out your copyright notice in the Description page of Project Settings.

#include "RSBossStatusViewModel.h"

#include "RSBossCharacter.h"
#include "RSHealthComponent.h"

void URSBossStatusViewModel::BeginDestroy()
{
	RequestHealthBarShakeReset();
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

	InitializeViewModel(BossCharacter->GetHealthComponent(), BossCharacter->GetBossName(), BossCharacter->GetHealthLayerCount());
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

void URSBossStatusViewModel::InitializeViewModel(URSHealthComponent* InHealthComponent, const FText& InBossName, int32 InHealthLayerCount)
{
	// 같은 원본을 다시 등록할 때에도 표시 설정을 먼저 반영합니다
	HealthLayerCount = FMath::Max(InHealthLayerCount, 1);

	if (IsValid(InHealthComponent) && HealthComponent.Get() == InHealthComponent)
	{
		UE_MVVM_SET_PROPERTY_VALUE(BossName, InBossName);
		UpdateHealthValues();
		UE_MVVM_SET_PROPERTY_VALUE(bIsVisible, true);
		return;
	}

	RequestHealthBarShakeReset();
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
	RequestHealthBarShakeReset();
	DisconnectHealthComponent();
	ResetStatusValues();
}

void URSBossStatusViewModel::RequestHealthBarShake(float Strength)
{
	const float SafeStrength = FMath::IsFinite(Strength) ? FMath::Clamp(Strength, 0.0f, 1.0f) : 0.0f;
	if (SafeStrength <= 0.0f)
	{
		return;
	}

	OnHealthBarShakeRequested.Broadcast(SafeStrength);
}

void URSBossStatusViewModel::HandleHealthChanged(URSHealthComponent* InHealthComponent, float OldValue, float NewValue)
{
	if (HealthComponent.Get() != InHealthComponent)
	{
		return;
	}

	ApplyHealthValues(NewValue, InHealthComponent->GetMaxHealth());

	if (OldValue > NewValue)
	{
		RequestHealthBarShake(1.0f);
	}
}

void URSBossStatusViewModel::HandleMaxHealthChanged(URSHealthComponent* InHealthComponent, float, float NewValue)
{
	if (HealthComponent.Get() != InHealthComponent)
	{
		return;
	}

	ApplyHealthValues(InHealthComponent->GetHealth(), NewValue);
}

void URSBossStatusViewModel::UpdateHealthValues()
{
	URSHealthComponent* CurrentHealthComponent = HealthComponent.Get();
	if (!IsValid(CurrentHealthComponent))
	{
		RequestHealthBarShakeReset();
		ResetStatusValues();
		return;
	}

	ApplyHealthValues(CurrentHealthComponent->GetHealth(), CurrentHealthComponent->GetMaxHealth());
}

void URSBossStatusViewModel::ApplyHealthValues(float InHealth, float InMaxHealth)
{
	const float SafeMaxHealth = FMath::IsFinite(InMaxHealth) ? FMath::Max(InMaxHealth, 0.0f) : 0.0f;
	const float SafeHealth = FMath::IsFinite(InHealth) ? FMath::Clamp(InHealth, 0.0f, SafeMaxHealth) : 0.0f;
	const int32 LayerCount = FMath::Max(HealthLayerCount, 1);
	int32 NewRemainingLayerCount = 0;
	float NewCurrentLayerPercent = 0.0f;

	if (SafeMaxHealth > 0.0f && SafeHealth > 0.0f)
	{
		// float 체력의 나눗셈 오차가 정수 경계를 넘지 않도록 중간 계산은 double로 수행합니다
		const double ScaledHealth = static_cast<double>(SafeHealth) * LayerCount / SafeMaxHealth;
		NewRemainingLayerCount = static_cast<int32>(FMath::Clamp<int64>(FMath::CeilToInt64(ScaledHealth), 1, LayerCount));

		// 비정수 경계도 Health와 같은 float 표현으로 비교하며 임의 epsilon으로 작은 잔여 체력을 제거하지 않습니다
		const float PreviousBoundary = static_cast<float>(static_cast<double>(SafeMaxHealth) * (NewRemainingLayerCount - 1) / LayerCount);
		if (NewRemainingLayerCount > 1 && SafeHealth < SafeMaxHealth && SafeHealth <= PreviousBoundary)
		{
			--NewRemainingLayerCount;
		}

		NewCurrentLayerPercent = static_cast<float>(FMath::Clamp(ScaledHealth - (NewRemainingLayerCount - 1), 0.0, 1.0));
	}

	const bool bShowLayerCount = NewRemainingLayerCount >= 2;
	const FText NewLayerCountText = bShowLayerCount ? FText::AsCultureInvariant(FString::Printf(TEXT("x%d"), NewRemainingLayerCount)) : FText::GetEmpty();
	const int32 NewCurrentLayerColorIndex = NewRemainingLayerCount > 0 ? (LayerCount - NewRemainingLayerCount) % 4 : 0;
	UE_MVVM_SET_PROPERTY_VALUE(Health, SafeHealth);
	UE_MVVM_SET_PROPERTY_VALUE(MaxHealth, SafeMaxHealth);
	UE_MVVM_SET_PROPERTY_VALUE(HealthNormalized, SafeMaxHealth > 0.0f ? SafeHealth / SafeMaxHealth : 0.0f);
	UE_MVVM_SET_PROPERTY_VALUE(CurrentLayerPercent, NewCurrentLayerPercent);
	UE_MVVM_SET_PROPERTY_VALUE(CurrentLayerColorIndex, NewCurrentLayerColorIndex);
	UE_MVVM_SET_PROPERTY_VALUE(RemainingLayerCount, NewRemainingLayerCount);
	UE_MVVM_SET_PROPERTY_VALUE(LayerCountText, NewLayerCountText);
	UE_MVVM_SET_PROPERTY_VALUE(LayerCountVisibility, bShowLayerCount ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void URSBossStatusViewModel::ResetStatusValues()
{
	UE_MVVM_SET_PROPERTY_VALUE(BossName, FText::GetEmpty());
	HealthLayerCount = 1;
	ApplyHealthValues(0.0f, 0.0f);
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

void URSBossStatusViewModel::RequestHealthBarShakeReset()
{
	OnHealthBarShakeResetRequested.Broadcast();
}
