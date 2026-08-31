// Fill out your copyright notice in the Description page of Project Settings.

#include "RSLocalPlayerViewModelSubsystem.h"

void URSLocalPlayerViewModelSubsystem::Deinitialize()
{
	ViewModels.Empty();

	Super::Deinitialize();
}

UMVVMViewModelBase* URSLocalPlayerViewModelSubsystem::GetViewModelByClass(TSubclassOf<UMVVMViewModelBase> ViewModelClass) const
{
	if (!ViewModelClass)
	{
		return nullptr;
	}

	if (const TObjectPtr<UMVVMViewModelBase>* FoundViewModel = ViewModels.Find(ViewModelClass))
	{
		return FoundViewModel->Get();
	}

	return nullptr;
}

UMVVMViewModelBase* URSLocalPlayerViewModelSubsystem::GetOrCreateViewModelByClass(TSubclassOf<UMVVMViewModelBase> ViewModelClass)
{
	if (!ViewModelClass)
	{
		return nullptr;
	}

	if (UMVVMViewModelBase* FoundViewModel = GetViewModelByClass(ViewModelClass))
	{
		return FoundViewModel;
	}

	UMVVMViewModelBase* NewViewModel = NewObject<UMVVMViewModelBase>(this, ViewModelClass);
	if (!NewViewModel)
	{
		return nullptr;
	}

	ViewModels.Add(ViewModelClass, NewViewModel);

	return NewViewModel;
}
