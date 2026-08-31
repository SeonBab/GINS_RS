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
	virtual void BeginDestroy() override;

public:
	/** HealthComponent를 데이터 원본으로 연결하고 현재 체력 값을 동기화합니다 */
	void InitializeViewModel(URSHealthComponent* InHealthComponent);

	/** HealthComponent의 이벤트 연결을 해제하고 체력 값을 초기화합니다 */
	void UninitializeViewModel();

private:
	UFUNCTION()
	void HandleHealthChanged(URSHealthComponent* InHealthComponent, float OldValue, float NewValue);

	UFUNCTION()
	void HandleMaxHealthChanged(URSHealthComponent* InHealthComponent, float OldValue, float NewValue);

	void UpdateHealthValues();

	void ResetHealthValues();

	void DisconnectHealthComponent();

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<URSHealthComponent> HealthComponent;

	UPROPERTY(Transient, BlueprintReadOnly, FieldNotify, Category = "RS|Player Status", meta = (AllowPrivateAccess = "true"))
	float Health = 0.0f;

	UPROPERTY(Transient, BlueprintReadOnly, FieldNotify, Category = "RS|Player Status", meta = (AllowPrivateAccess = "true"))
	float MaxHealth = 0.0f;

	UPROPERTY(Transient, BlueprintReadOnly, FieldNotify, Category = "RS|Player Status", meta = (AllowPrivateAccess = "true"))
	float HealthNormalized = 0.0f;
};
