// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "RSLocalPlayerViewModelSubsystem.generated.h"

class URSLocalPlayerViewModelBase;

/**
 * 로컬 플레이어가 공유하는 ViewModel을 클래스별로 생성하고 보관합니다
 * 이 저장소는 LocalPlayer 수명을 따르므로 World가 교체되어도 유지됩니다. 따라서 정리되는 World의 데이터 원본을 직접 해제해야 합니다
 */
UCLASS()
class RS_API URSLocalPlayerViewModelSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	/** World 정리 시점을 관찰하기 위해 이벤트를 구독합니다 */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** LocalPlayer 종료 시 보관한 ViewModel 참조를 해제합니다 */
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

	/** 현재 로컬 플레이어의 ViewModel에 제공할 데이터 원본을 등록합니다 */
	void RegisterSource(UObject* Source);

	/** 등록한 데이터 원본을 제거하고 이를 관찰하는 ViewModel에 해제를 알립니다 */
	void UnregisterSource(UObject* Source);

private:
	/**
	 * 정리되는 World에 속한 데이터 원본을 모두 해제합니다
	 * 이 시점에는 원본이 아직 유효하므로 각 ViewModel이 자신이 관찰하던 원본인지 판단할 수 있습니다
	 */
	void HandleWorldBeginTearDown(UWorld* World);

	/** 지정한 World에 속하지 않은 데이터 원본을 해제합니다 */
	void UnregisterSourcesOutsideWorld(const UWorld* World);

	/** 이미 파괴된 데이터 원본 참조를 목록에서 제거합니다 */
	void RemoveInvalidSources();

	/** 보관 중인 모든 ViewModel에 데이터 원본의 등록 또는 해제를 알립니다 */
	void NotifyViewModels(UObject* Source, bool bRegistered);

private:
	/** 로컬 플레이어가 공유하는 ViewModel 인스턴스입니다 */
	UPROPERTY(Transient)
	TMap<TSubclassOf<UMVVMViewModelBase>, TObjectPtr<UMVVMViewModelBase>> ViewModels;

	/** ViewModel 생성 순서와 관계없이 다시 전달할 현재 데이터 원본입니다 */
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<UObject>> Sources;

	/** World 정리 이벤트 구독을 해제하기 위한 핸들입니다 */
	FDelegateHandle WorldBeginTearDownHandle;

private:
	friend class FRSBossEncounterTimerTest;
};
