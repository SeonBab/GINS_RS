// Fill out your copyright notice in the Description page of Project Settings.

#include "RSLocalPlayerViewModelSubsystem.h"

#include "RSLocalPlayerViewModelBase.h"

void URSLocalPlayerViewModelSubsystem::Deinitialize()
{
	for (const TPair<TSubclassOf<UMVVMViewModelBase>, TObjectPtr<UMVVMViewModelBase>>& ViewModelPair : ViewModels)
	{
		URSLocalPlayerViewModelBase* ViewModel = Cast<URSLocalPlayerViewModelBase>(ViewModelPair.Value.Get());
		if (!ViewModel)
		{
			continue;
		}

		for (const TWeakObjectPtr<UObject>& SourceReference : Sources)
		{
			if (UObject* Source = SourceReference.Get())
			{
				ViewModel->HandleSourceUnregistered(Source);
			}
		}
	}

	Sources.Reset();
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

	if (URSLocalPlayerViewModelBase* LocalPlayerViewModel = Cast<URSLocalPlayerViewModelBase>(NewViewModel))
	{
		for (const TWeakObjectPtr<UObject>& SourceReference : Sources)
		{
			if (UObject* Source = SourceReference.Get())
			{
				LocalPlayerViewModel->HandleSourceRegistered(Source);
			}
		}
	}

	return NewViewModel;
}

void URSLocalPlayerViewModelSubsystem::RegisterSource(UObject* Source)
{
	if (!IsValid(Source))
	{
		return;
	}

	Sources.RemoveAll([](const TWeakObjectPtr<UObject>& SourceReference)
	{
		return !SourceReference.IsValid();
	});

	const bool bIsAlreadyRegistered = Sources.ContainsByPredicate([Source](const TWeakObjectPtr<UObject>& SourceReference)
	{
		return SourceReference.Get() == Source;
	});

	if (bIsAlreadyRegistered)
	{
		return;
	}

	Sources.Add(Source);

	for (const TPair<TSubclassOf<UMVVMViewModelBase>, TObjectPtr<UMVVMViewModelBase>>& ViewModelPair : ViewModels)
	{
		if (URSLocalPlayerViewModelBase* ViewModel = Cast<URSLocalPlayerViewModelBase>(ViewModelPair.Value.Get()))
		{
			ViewModel->HandleSourceRegistered(Source);
		}
	}
}

void URSLocalPlayerViewModelSubsystem::UnregisterSource(UObject* Source)
{
	if (!Source)
	{
		return;
	}

	const bool bWasRegistered = Sources.ContainsByPredicate([Source](const TWeakObjectPtr<UObject>& SourceReference)
	{
		return SourceReference.Get() == Source;
	});

	Sources.RemoveAll([Source](const TWeakObjectPtr<UObject>& SourceReference)
	{
		return !SourceReference.IsValid() || SourceReference.Get() == Source;
	});

	if (!bWasRegistered)
	{
		return;
	}

	for (const TPair<TSubclassOf<UMVVMViewModelBase>, TObjectPtr<UMVVMViewModelBase>>& ViewModelPair : ViewModels)
	{
		if (URSLocalPlayerViewModelBase* ViewModel = Cast<URSLocalPlayerViewModelBase>(ViewModelPair.Value.Get()))
		{
			ViewModel->HandleSourceUnregistered(Source);
		}
	}
}
