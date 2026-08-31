// Fill out your copyright notice in the Description page of Project Settings.

#include "RSPlayerHeadUpDisplay.h"

#include "Engine/LocalPlayer.h"
#include "MVVMSubsystem.h"
#include "MVVMViewModelBase.h"
#include "RSLocalPlayerViewModelSubsystem.h"
#include "RSPrimaryLayout.h"
#include "View/MVVMView.h"
#include "View/MVVMViewClass.h"

void ARSPlayerHeadUpDisplay::BeginPlay()
{
	Super::BeginPlay();

	CreatePrimaryLayout();
}

void ARSPlayerHeadUpDisplay::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (PrimaryLayout)
	{
		PrimaryLayout->RemoveFromParent();
		PrimaryLayout = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

URSPrimaryLayout* ARSPlayerHeadUpDisplay::GetPrimaryLayout() const
{
	return PrimaryLayout;
}

void ARSPlayerHeadUpDisplay::CreatePrimaryLayout()
{
	APlayerController* PlayerController = GetOwningPlayerController();
	if (!PlayerController || !PlayerController->IsLocalController() || !PrimaryLayoutClass)
	{
		return;
	}

	PrimaryLayout = CreateWidget<URSPrimaryLayout>(PlayerController, PrimaryLayoutClass);
	if (!PrimaryLayout)
	{
		return;
	}

	SetSharedViewModels();
	PrimaryLayout->AddToViewport();
}

void ARSPlayerHeadUpDisplay::SetSharedViewModels()
{
	if (!PrimaryLayout)
	{
		return;
	}

	APlayerController* PlayerController = GetOwningPlayerController();
	ULocalPlayer* LocalPlayer = PlayerController ? PlayerController->GetLocalPlayer() : nullptr;
	if (!LocalPlayer)
	{
		return;
	}

	URSLocalPlayerViewModelSubsystem* ViewModelSubsystem = LocalPlayer->GetSubsystem<URSLocalPlayerViewModelSubsystem>();
	UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(PrimaryLayout);
	if (!ViewModelSubsystem || !View || !View->GetViewClass())
	{
		return;
	}

	for (const FMVVMViewClass_Source& Source : View->GetViewClass()->GetSources())
	{
		if (!Source.IsViewModel() || !Source.CanBeSet())
		{
			continue;
		}

		UClass* ViewModelClass = Source.GetSourceClass();
		if (!ViewModelClass || !ViewModelClass->IsChildOf(UMVVMViewModelBase::StaticClass()))
		{
			continue;
		}

		UMVVMViewModelBase* ViewModel = ViewModelSubsystem->GetOrCreateViewModelByClass(ViewModelClass);
		if (ViewModel)
		{
			PrimaryLayout->SetViewModel(ViewModel);
		}
	}
}
