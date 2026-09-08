// Fill out your copyright notice in the Description page of Project Settings.

#include "RSLocalPlayerViewModelSubsystem.h"

#include "Engine/World.h"
#include "RSLocalPlayerViewModelBase.h"

void URSLocalPlayerViewModelSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Actor의 EndPlay와 파괴 순서는 보장되지 않으므로 소유자를 거치지 않는 단일 지점에서 해제합니다
	WorldBeginTearDownHandle = FWorldDelegates::OnWorldBeginTearDown.AddUObject(this, &ThisClass::HandleWorldBeginTearDown);
}

void URSLocalPlayerViewModelSubsystem::Deinitialize()
{
	FWorldDelegates::OnWorldBeginTearDown.Remove(WorldBeginTearDownHandle);
	WorldBeginTearDownHandle.Reset();

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

	RemoveInvalidSources();

	// World 교체 시 이전 World의 해제를 놓쳤더라도 새 원본이 들어오는 시점에 남은 원본을 정리합니다
	UnregisterSourcesOutsideWorld(Source->GetWorld());

	// 이미 등록한 원본이라도 다시 알립니다
	// Pawn을 소유하는 시점에는 아직 준비되지 않은 구성 요소가 있을 수 있어, 준비를 마친 게임 객체가 같은 경로로 다시 알릴 수 있어야 합니다
	if (!Sources.Contains(Source))
	{
		Sources.Add(Source);
	}

	NotifyViewModels(Source, true);
}

void URSLocalPlayerViewModelSubsystem::UnregisterSource(UObject* Source)
{
	if (!Source)
	{
		return;
	}

	const bool bWasRegistered = Sources.Contains(Source);

	Sources.RemoveAll([Source](const TWeakObjectPtr<UObject>& SourceReference)
	{
		return !SourceReference.IsValid() || SourceReference.Get() == Source;
	});

	if (!bWasRegistered)
	{
		return;
	}

	NotifyViewModels(Source, false);
}

void URSLocalPlayerViewModelSubsystem::HandleWorldBeginTearDown(UWorld* World)
{
	if (!World)
	{
		return;
	}

	TArray<UObject*> TearingDownSources;
	for (const TWeakObjectPtr<UObject>& SourceReference : Sources)
	{
		UObject* Source = SourceReference.Get();
		if (Source && Source->GetWorld() == World)
		{
			TearingDownSources.Add(Source);
		}
	}

	// UnregisterSource가 목록을 수정하므로 대상을 먼저 모읍니다
	for (UObject* Source : TearingDownSources)
	{
		UnregisterSource(Source);
	}

	RemoveInvalidSources();
}

void URSLocalPlayerViewModelSubsystem::UnregisterSourcesOutsideWorld(const UWorld* World)
{
	TArray<UObject*> OutsideSources;
	for (const TWeakObjectPtr<UObject>& SourceReference : Sources)
	{
		UObject* Source = SourceReference.Get();
		if (Source && Source->GetWorld() != World)
		{
			OutsideSources.Add(Source);
		}
	}

	for (UObject* Source : OutsideSources)
	{
		UnregisterSource(Source);
	}
}

void URSLocalPlayerViewModelSubsystem::RemoveInvalidSources()
{
	Sources.RemoveAll([](const TWeakObjectPtr<UObject>& SourceReference)
	{
		return !SourceReference.IsValid();
	});
}

void URSLocalPlayerViewModelSubsystem::NotifyViewModels(UObject* Source, bool bRegistered)
{
	for (const TPair<TSubclassOf<UMVVMViewModelBase>, TObjectPtr<UMVVMViewModelBase>>& ViewModelPair : ViewModels)
	{
		URSLocalPlayerViewModelBase* ViewModel = Cast<URSLocalPlayerViewModelBase>(ViewModelPair.Value.Get());
		if (!ViewModel)
		{
			continue;
		}

		if (bRegistered)
		{
			ViewModel->HandleSourceRegistered(Source);
		}
		else
		{
			ViewModel->HandleSourceUnregistered(Source);
		}
	}
}
