// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "RSPlayerStatusViewModel.generated.h"

class URSHealthComponent;

/** 플레이어 상태를 사용자 인터페이스에 제공하고 값 변경을 알립니다 */
UCLASS(BlueprintType)
class RS_API URSPlayerStatusViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/** 제거되기 전에 HealthComponent의 이벤트 연결을 해제합니다 */
	virtual void BeginDestroy() override;

public:
	/** HealthComponent를 데이터 원본으로 연결하고 현재 체력 값을 동기화합니다 */
	void InitializeViewModel(URSHealthComponent* InHealthComponent);

	/** HealthComponent의 이벤트 연결을 해제하고 체력 값을 초기화합니다 */
	void UninitializeViewModel();

private:
	/** 현재 체력 변경을 사용자 인터페이스에 제공하는 값에 반영합니다 */
	UFUNCTION()
	void HandleHealthChanged(URSHealthComponent* InHealthComponent, float OldValue, float NewValue);

	/** 최대 체력 변경을 사용자 인터페이스에 제공하는 값에 반영합니다 */
	UFUNCTION()
	void HandleMaxHealthChanged(URSHealthComponent* InHealthComponent, float OldValue, float NewValue);

	/** 연결된 HealthComponent의 현재 상태를 모든 공개 체력 값에 동기화합니다 */
	void UpdateHealthValues();

	/** 연결된 데이터 원본이 없을 때 공개 체력 값을 초기 상태로 되돌립니다 */
	void ResetHealthValues();

	/** HealthComponent의 생명주기를 유지하지 않고 등록한 이벤트만 해제합니다 */
	void DisconnectHealthComponent();

private:
	/** 플레이어 상태 값을 관찰하는 데이터 원본입니다 */
	UPROPERTY(Transient)
	TWeakObjectPtr<URSHealthComponent> HealthComponent;

	/** 사용자 인터페이스에 제공하는 현재 체력입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, FieldNotify, Category = "RS|Player Status", meta = (AllowPrivateAccess = "true"))
	float Health = 0.0f;

	/** 사용자 인터페이스에 제공하는 최대 체력입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, FieldNotify, Category = "RS|Player Status", meta = (AllowPrivateAccess = "true"))
	float MaxHealth = 0.0f;

	/** ProgressBar 등에 사용할 0부터 1까지 범위의 체력 비율입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, FieldNotify, Category = "RS|Player Status", meta = (AllowPrivateAccess = "true"))
	float HealthNormalized = 0.0f;
};
