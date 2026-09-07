// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SlateWrapperTypes.h"
#include "RSLocalPlayerViewModelBase.h"
#include "RSBossStatusViewModel.generated.h"

class URSHealthComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRSHealthBarShakeRequestedSignature, float, Strength);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRSHealthBarShakeResetRequestedSignature);

/** 현재 보스의 상태를 사용자 인터페이스에 제공하고 값 변경을 알립니다 */
UCLASS(BlueprintType)
class RS_API URSBossStatusViewModel : public URSLocalPlayerViewModelBase
{
	GENERATED_BODY()

public:
	/** 제거되기 전에 HealthComponent의 이벤트 연결을 해제합니다 */
	virtual void BeginDestroy() override;

	/** BossCharacter가 데이터 원본으로 등록되면 이름과 체력 상태를 연결합니다 */
	virtual void HandleSourceRegistered(UObject* Source) override;

	/** 현재 BossCharacter가 데이터 원본에서 해제되면 이름과 체력 상태를 정리합니다 */
	virtual void HandleSourceUnregistered(UObject* Source) override;

public:
	/** 보스의 HealthComponent를 연결하고 이름과 체력 값을 동기화합니다 */
	void InitializeViewModel(URSHealthComponent* InHealthComponent, const FText& InBossName, int32 InHealthLayerCount);

	/** HealthComponent의 이벤트 연결을 해제하고 이름과 체력 값을 초기화합니다 */
	void UninitializeViewModel();

	/** 현재 활성 레이어의 빨강·주황·노랑·초록 반복 순번을 반환합니다 */
	int32 GetCurrentLayerColorIndex() const { return CurrentLayerColorIndex; }

	/** 현재 활성 레이어를 포함하여 남은 레이어 수를 반환합니다 */
	int32 GetRemainingLayerCount() const { return RemainingLayerCount; }

	/** 원인과 무관한 보스 체력 사용자 인터페이스 Shake를 요청합니다 */
	UFUNCTION(BlueprintCallable, Category = "RS|Boss Status")
	void RequestHealthBarShake(float Strength);

public:
	/** 보스 체력 사용자 인터페이스에 일회성 Shake와 0부터 1까지의 진폭 강도를 요청합니다 */
	UPROPERTY(BlueprintAssignable, Category = "RS|Boss Status")
	FRSHealthBarShakeRequestedSignature OnHealthBarShakeRequested;

	/** 원본 생명주기가 바뀌기 전에 진행 중인 Shake 정리를 요청합니다 */
	UPROPERTY(BlueprintAssignable, Category = "RS|Boss Status")
	FRSHealthBarShakeResetRequestedSignature OnHealthBarShakeResetRequested;

private:
	/** 현재 체력 변경을 사용자 인터페이스에 제공하는 값에 반영합니다 */
	UFUNCTION()
	void HandleHealthChanged(URSHealthComponent* InHealthComponent, float OldValue, float NewValue);

	/** 최대 체력 변경을 사용자 인터페이스에 제공하는 값에 반영합니다 */
	UFUNCTION()
	void HandleMaxHealthChanged(URSHealthComponent* InHealthComponent, float OldValue, float NewValue);

	/** 연결된 HealthComponent의 현재 상태를 모든 공개 체력 값에 동기화합니다 */
	void UpdateHealthValues();

	/** 체력 원본에서 전체 비율과 레이어 표시값을 함께 계산하며 표시 이력은 보관하지 않습니다 */
	void ApplyHealthValues(float InHealth, float InMaxHealth);

	/** 연결된 데이터 원본이 없을 때 이름과 체력 및 표시 여부를 초기화합니다 */
	void ResetStatusValues();

	/** HealthComponent의 생명주기를 유지하지 않고 등록한 이벤트만 해제합니다 */
	void DisconnectHealthComponent();

	/** 원본 해제나 교체 전에 보스 체력 사용자 인터페이스의 일회성 표현을 정리하도록 요청합니다 */
	void RequestHealthBarShakeReset();

private:
	friend class FRSBossHealthLayersTest;

	/** 현재 원본의 고정 표시 설정이며 게임플레이 체력 상태가 아닙니다 */
	int32 HealthLayerCount = 1;

	/** 현재 보스 상태 값을 관찰하는 데이터 원본입니다 */
	UPROPERTY(Transient)
	TWeakObjectPtr<URSHealthComponent> HealthComponent;

	/** 사용자 인터페이스에 제공하는 현재 보스의 이름입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, FieldNotify, Category = "RS|Boss Status", meta = (AllowPrivateAccess = "true"))
	FText BossName;

	/** 사용자 인터페이스에 제공하는 현재 체력입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, FieldNotify, Category = "RS|Boss Status", meta = (AllowPrivateAccess = "true"))
	float Health = 0.0f;

	/** 사용자 인터페이스에 제공하는 최대 체력입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, FieldNotify, Category = "RS|Boss Status", meta = (AllowPrivateAccess = "true"))
	float MaxHealth = 0.0f;

	/** ProgressBar 등에 사용할 0부터 1까지 범위의 체력 비율입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, FieldNotify, Category = "RS|Boss Status", meta = (AllowPrivateAccess = "true"))
	float HealthNormalized = 0.0f;

	/** 현재 활성 레이어의 비율이며 정확한 소진 경계에서는 다음 레이어의 1을 제공합니다 */
	UPROPERTY(Transient, BlueprintReadOnly, FieldNotify, Category = "RS|Boss Status", meta = (AllowPrivateAccess = "true"))
	float CurrentLayerPercent = 0.0f;

	/** 현재 활성 레이어를 포함하여 남은 레이어 수입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, FieldNotify, Category = "RS|Boss Status", meta = (AllowPrivateAccess = "true"))
	int32 RemainingLayerCount = 0;

	/** 소진된 레이어 수를 기준으로 빨강·주황·노랑·초록을 반복하는 0부터 3까지의 순번입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, FieldNotify, Category = "RS|Boss Status", meta = (AllowPrivateAccess = "true"))
	int32 CurrentLayerColorIndex = 0;

	/** 두 레이어 이상일 때만 xN을 제공하며 마지막 레이어와 체력 0에서는 비웁니다 */
	UPROPERTY(Transient, BlueprintReadOnly, FieldNotify, Category = "RS|Boss Status", meta = (AllowPrivateAccess = "true"))
	FText LayerCountText;

	/** 보스 사용자 인터페이스 전체 표시와 별도로 카운트 텍스트만 숨깁니다 */
	UPROPERTY(Transient, BlueprintReadOnly, FieldNotify, Category = "RS|Boss Status", meta = (AllowPrivateAccess = "true"))
	ESlateVisibility LayerCountVisibility = ESlateVisibility::Collapsed;

	/** 보스 체력 사용자 인터페이스를 표시할지 나타냅니다 */
	UPROPERTY(Transient, BlueprintReadOnly, FieldNotify, Category = "RS|Boss Status", meta = (AllowPrivateAccess = "true"))
	bool bIsVisible = false;
};
