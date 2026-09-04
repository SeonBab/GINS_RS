// Fill out your copyright notice in the Description page of Project Settings.


#include "RSAbilityBarWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Engine/LocalPlayer.h"
#include "RSAbilitySlotViewModel.h"
#include "RSAbilitySlotWidget.h"
#include "RSLocalPlayerViewModelSubsystem.h"
#include "RSPlayerAbilityViewModel.h"

void URSAbilityBarWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	InitializeSlotViewModels();
}

void URSAbilityBarWidget::InitializeSlotViewModels()
{
	// 루트 레이아웃의 Manual 소스만 HUD가 주입하므로, 중첩된 이 위젯은 저장소에서 직접 ViewModel을 얻습니다
	const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	URSLocalPlayerViewModelSubsystem* ViewModelSubsystem = LocalPlayer ? LocalPlayer->GetSubsystem<URSLocalPlayerViewModelSubsystem>() : nullptr;
	if (!ViewModelSubsystem || !WidgetTree)
	{
		return;
	}

	URSPlayerAbilityViewModel* PlayerAbilityViewModel = ViewModelSubsystem->GetOrCreateViewModel<URSPlayerAbilityViewModel>();
	if (!PlayerAbilityViewModel)
	{
		return;
	}

	WidgetTree->ForEachWidget([PlayerAbilityViewModel](UWidget* Widget)
	{
		URSAbilitySlotWidget* SlotWidget = Cast<URSAbilitySlotWidget>(Widget);
		if (!SlotWidget)
		{
			return;
		}

		if (URSAbilitySlotViewModel* SlotViewModel = PlayerAbilityViewModel->GetOrCreateSlotViewModel(SlotWidget->GetInputTag(), SlotWidget->GetIconMaterial()))
		{
			SlotWidget->SetViewModel(SlotViewModel);
		}
	});
}
