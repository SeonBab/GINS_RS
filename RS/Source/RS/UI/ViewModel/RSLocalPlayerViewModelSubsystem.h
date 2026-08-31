// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "RSLocalPlayerViewModelSubsystem.generated.h"

/** 로컬 플레이어가 공유하는 ViewModel을 클래스별로 생성하고 보관합니다 */
UCLASS()
class RS_API URSLocalPlayerViewModelSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

public:
	/** 생성된 ViewModel을 타입으로 조회합니다 */
	template<typename T>
	T* GetViewModel() const
	{
		static_assert(TIsDerivedFrom<T, UMVVMViewModelBase>::Value, "T must derive from UMVVMViewModelBase");
		return Cast<T>(GetViewModelByClass(T::StaticClass()));
	}

	/** ViewModel을 타입으로 조회하고 없으면 생성합니다 */
	template<typename T>
	T* GetOrCreateViewModel()
	{
		static_assert(TIsDerivedFrom<T, UMVVMViewModelBase>::Value, "T must derive from UMVVMViewModelBase");
		return Cast<T>(GetOrCreateViewModelByClass(T::StaticClass()));
	}

	/** 생성된 ViewModel을 클래스로 조회합니다 */
	UMVVMViewModelBase* GetViewModelByClass(TSubclassOf<UMVVMViewModelBase> ViewModelClass) const;

	/** ViewModel을 클래스로 조회하고 없으면 생성합니다 */
	UMVVMViewModelBase* GetOrCreateViewModelByClass(TSubclassOf<UMVVMViewModelBase> ViewModelClass);

private:
	/** 로컬 플레이어가 공유하는 ViewModel 인스턴스입니다 */
	UPROPERTY(Transient)
	TMap<TSubclassOf<UMVVMViewModelBase>, TObjectPtr<UMVVMViewModelBase>> ViewModels;
};
