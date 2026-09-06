// Fill out your copyright notice in the Description page of Project Settings.


#include "RSAbilitySlotViewModel.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "RSAbilityDefinition.h"

void URSAbilitySlotViewModel::Tick(float DeltaTime)
{
	if (bIsOnCooldown)
	{
		UpdateCooldownValues();

		// 만료 판정을 월드 시간으로 하여 프레임 누적 오차가 남은 시간에 쌓이지 않게 합니다
		if (CooldownRemaining <= 0.0f)
		{
			FinishCooldown();
		}
	}

	if (bIsReadyFlashing)
	{
		UpdateReadyFlash();
	}
}

bool URSAbilitySlotViewModel::IsTickable() const
{
	return bIsOnCooldown || bIsReadyFlashing;
}

TStatId URSAbilitySlotViewModel::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URSAbilitySlotViewModel, STATGROUP_Tickables);
}

UWorld* URSAbilitySlotViewModel::GetTickableGameObjectWorld() const
{
	// LocalPlayer 저장소가 소유하므로 Outer를 따라 올라가야 월드에 닿습니다
	return GEngine ? GEngine->GetWorldFromContextObject(this, EGetWorldErrorMode::ReturnNull) : nullptr;
}

const FGameplayTag& URSAbilitySlotViewModel::GetInputTag() const
{
	return InputTag;
}

void URSAbilitySlotViewModel::SetInputTag(const FGameplayTag& InInputTag)
{
	InputTag = InInputTag;
}

void URSAbilitySlotViewModel::SetPresentationConfig(const FRSAbilitySlotPresentationConfig& InPresentationConfig)
{
	// 부모가 달라지면 기존 인스턴스를 재사용할 수 없으므로 다음 표시에서 새로 만듭니다
	if (PresentationConfig.IconMaterial != InPresentationConfig.IconMaterial)
	{
		IconMaterialInstance = nullptr;
	}

	PresentationConfig = InPresentationConfig;
}

void URSAbilitySlotViewModel::SetInputKeyText(const FText& InInputKeyText)
{
	UE_MVVM_SET_PROPERTY_VALUE(InputKeyText, InInputKeyText);
}

void URSAbilitySlotViewModel::SetPresentation(const URSAbilityDefinition* Definition, UTexture2D* InIconTexture)
{
	// 슬롯이 대표하는 어빌리티가 달라질 수 있으므로 이전 어빌리티의 연출을 이어가지 않습니다
	StopReadyFlash();

	if (!Definition)
	{
		UE_MVVM_SET_PROPERTY_VALUE(DisplayName, FText::GetEmpty());
		UE_MVVM_SET_PROPERTY_VALUE(Icon, FSlateBrush());
		UE_MVVM_SET_PROPERTY_VALUE(bHasAbility, false);

		return;
	}

	// Definition은 표시 방식을 모르는 원본 텍스처만 제공하므로 Brush 구성은 표시 계층인 이곳에서 합니다
	FSlateBrush IconBrush;

	if (PresentationConfig.IconMaterial)
	{
		if (!IconMaterialInstance)
		{
			IconMaterialInstance = UMaterialInstanceDynamic::Create(PresentationConfig.IconMaterial, this);
		}

		if (IconMaterialInstance)
		{
			IconMaterialInstance->SetTextureParameterValue(TEXT("IconTexture"), InIconTexture);
			IconBrush.SetResourceObject(IconMaterialInstance);
		}
	}

	// 머티리얼을 지정하지 않았으면 텍스처를 그대로 그리며 채도 변경만 동작하지 않습니다
	if (!IconBrush.GetResourceObject())
	{
		IconBrush.SetResourceObject(InIconTexture);
	}

	UE_MVVM_SET_PROPERTY_VALUE(DisplayName, Definition->DisplayName);
	UE_MVVM_SET_PROPERTY_VALUE(Icon, IconBrush);
	UE_MVVM_SET_PROPERTY_VALUE(bHasAbility, true);

	UpdateIconDesaturation();
}

void URSAbilitySlotViewModel::SetCooldown(float InCooldownEndTime, float InCooldownDuration)
{
	// 연출이 끝나기 전에 다시 쿨다운이 시작되면 밝아진 채로 남지 않도록 중단합니다
	StopReadyFlash();

	UE_MVVM_SET_PROPERTY_VALUE(CooldownEndTime, InCooldownEndTime);
	UE_MVVM_SET_PROPERTY_VALUE(CooldownDuration, InCooldownDuration);

	// bIsOnCooldown이 Tick 여부를 결정하므로 갱신할 값을 먼저 넣은 뒤 켭니다
	UE_MVVM_SET_PROPERTY_VALUE(bIsOnCooldown, true);

	UpdateIconDesaturation();

	// 첫 Tick을 기다리면 한 프레임 동안 이전 값이 보이므로 즉시 반영합니다
	UpdateCooldownValues();
}

void URSAbilitySlotViewModel::FinishCooldown()
{
	// Tick과 쿨다운 태그 제거가 같은 만료를 두 번 알릴 수 있으므로 연출을 다시 시작하지 않습니다
	if (!bIsOnCooldown)
	{
		return;
	}

	StartReadyFlash();
	ClearCooldown();
}

void URSAbilitySlotViewModel::ClearCooldown()
{
	// 이 값이 false가 되면 다음 프레임부터 Tick 대상에서 빠집니다
	UE_MVVM_SET_PROPERTY_VALUE(bIsOnCooldown, false);

	UE_MVVM_SET_PROPERTY_VALUE(CooldownRemaining, 0.0f);
	UE_MVVM_SET_PROPERTY_VALUE(CooldownPercent, 0.0f);
	UE_MVVM_SET_PROPERTY_VALUE(CooldownEndTime, 0.0f);
	UE_MVVM_SET_PROPERTY_VALUE(CooldownDuration, 0.0f);

	UpdateIconDesaturation();
}

void URSAbilitySlotViewModel::UpdateCooldownValues()
{
	const UWorld* World = GetTickableGameObjectWorld();
	const float Remaining = World ? FMath::Max(CooldownEndTime - World->GetTimeSeconds(), 0.0f) : 0.0f;

	UE_MVVM_SET_PROPERTY_VALUE(CooldownRemaining, Remaining);

	const float Percent = CooldownDuration > 0.0f ? FMath::Clamp(Remaining / CooldownDuration, 0.0f, 1.0f) : 0.0f;
	UE_MVVM_SET_PROPERTY_VALUE(CooldownPercent, Percent);
}

void URSAbilitySlotViewModel::UpdateIconDesaturation()
{
	if (!IconMaterialInstance)
	{
		return;
	}

	// Brush가 같은 머티리얼 인스턴스를 계속 가리키므로 값만 바꾸면 즉시 반영되고 변경 통지가 필요 없습니다
	IconMaterialInstance->SetScalarParameterValue(TEXT("Desaturation"), bIsOnCooldown ? 1.0f : 0.0f);
}

void URSAbilitySlotViewModel::StartReadyFlash()
{
	const UWorld* World = GetTickableGameObjectWorld();
	const float TotalDuration = PresentationConfig.ReadyFlashInDuration + PresentationConfig.ReadyFlashOutDuration;
	if (!World || TotalDuration <= 0.0f)
	{
		return;
	}

	ReadyFlashStartTime = World->GetTimeSeconds();
	bIsReadyFlashing = true;

	// 첫 Tick을 기다리면 한 프레임 동안 이전 밝기가 남으므로 즉시 반영합니다
	UpdateReadyFlash();
}

void URSAbilitySlotViewModel::StopReadyFlash()
{
	if (!bIsReadyFlashing)
	{
		return;
	}

	bIsReadyFlashing = false;
	ReadyFlashStartTime = 0.0f;

	if (IconMaterialInstance)
	{
		IconMaterialInstance->SetScalarParameterValue(TEXT("FlashAmount"), 0.0f);
	}
}

void URSAbilitySlotViewModel::UpdateReadyFlash()
{
	const UWorld* World = GetTickableGameObjectWorld();
	const float InDuration = PresentationConfig.ReadyFlashInDuration;
	const float OutDuration = PresentationConfig.ReadyFlashOutDuration;
	const float Elapsed = World ? World->GetTimeSeconds() - ReadyFlashStartTime : 0.0f;

	if (!World || Elapsed >= InDuration + OutDuration)
	{
		StopReadyFlash();

		return;
	}

	if (!IconMaterialInstance)
	{
		return;
	}

	// 밝아지는 구간과 돌아오는 구간을 나눠 각각의 시간을 그대로 반영합니다
	// 설정한 시간이 화면에서 보이는 시간과 일치하도록 가감속 없이 선형으로 보간합니다
	float FlashAmount = 0.0f;
	if (Elapsed < InDuration)
	{
		FlashAmount = InDuration > 0.0f ? Elapsed / InDuration : 1.0f;
	}
	else
	{
		FlashAmount = OutDuration > 0.0f ? 1.0f - (Elapsed - InDuration) / OutDuration : 0.0f;
	}

	IconMaterialInstance->SetScalarParameterValue(TEXT("FlashAmount"), FMath::Clamp(FlashAmount, 0.0f, 1.0f));
}
