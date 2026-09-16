// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RSLocalPlayerViewModelBase.h"
#include "RSDamageNumberViewModel.generated.h"

class URSAbilitySystemComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRSDamageNumberRequestedSignature, FText, DisplayDamageText, FVector, WorldAnchor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRSDamageNumberResetRequestedSignature);

/**
 * 로컬 플레이어가 적에게 가한 피해를 데미지 숫자 표시 요청으로 변환합니다
 * 데미지 숫자는 지속 상태가 아니라 일회성 사건이므로 FieldNotify 대신 요청 델리게이트를 제공합니다
 */
UCLASS(BlueprintType)
class RS_API URSDamageNumberViewModel : public URSLocalPlayerViewModelBase
{
	GENERATED_BODY()

public:
	/** 제거되기 전에 남은 구독을 정리합니다 */
	virtual void BeginDestroy() override;

	/** PlayerCharacter가 데이터 원본으로 등록되면 그 ASC의 피해 이벤트를 관찰합니다 */
	virtual void HandleSourceRegistered(UObject* Source) override;

	/** 현재 Source가 해제되면 관찰을 정리하고 표시 중인 숫자를 폐기하도록 요청합니다 */
	virtual void HandleSourceUnregistered(UObject* Source) override;

public:
	/**
	 * 피해 이벤트를 관찰할 ASC와 데미지 숫자의 시작 높이를 연결합니다
	 * 같은 ASC를 다시 전달하면 배선을 그대로 두고 반환하므로 같은 Source의 재등록에 안전합니다
	 * 같은 ASC를 사용하는 새 Pawn이 등록되면 시작 높이만 새 캡슐 높이로 갱신합니다
	 * nullptr 또는 유효하지 않은 높이는 아무 작업도 하지 않습니다. 해제 경로는 UninitializeViewModel 하나로 유지합니다
	 */
	void InitializeViewModel(URSAbilitySystemComponent* InAbilitySystemComponent, float InDamageNumberStartHeight);

	/**
	 * 연결한 ASC의 구독을 해제하고 표시 중인 숫자를 폐기하도록 요청합니다
	 * 이미 해제된 상태에서 다시 호출하면 아무 작업도 하지 않습니다
	 */
	void UninitializeViewModel();

public:
	/** 새 데미지 숫자 하나를 지정한 월드 앵커에 표시하도록 요청합니다 */
	UPROPERTY(BlueprintAssignable, Category = "RS|Damage Number")
	FRSDamageNumberRequestedSignature OnDamageNumberRequested;

	/** 이전 소유자의 표현이 남지 않도록 표시 중인 데미지 숫자를 모두 폐기하도록 요청합니다 */
	UPROPERTY(BlueprintAssignable, Category = "RS|Damage Number")
	FRSDamageNumberResetRequestedSignature OnDamageNumberResetRequested;

private:
	/** 기존 구독을 해제한 뒤 새 ASC의 피해 이벤트만 구독합니다 */
	void ConnectDamageSource(URSAbilitySystemComponent* InAbilitySystemComponent);

	/**
	 * ASC 델리게이트 연결만 해제합니다
	 * 표현 폐기 요청은 호출 시점마다 필요 여부가 달라서 이 함수에 결합하지 않습니다
	 */
	void DisconnectDamageSource();

	/** 적용된 피해량을 표시 문자열과 월드 앵커로 변환해 요청합니다 */
	UFUNCTION()
	void HandleDamageDealt(float AppliedDamage, FVector TargetGroundLocation);

private:
	/**
	 * 현재 관찰하는 Source이며 ASC와 별개의 identity입니다
	 * 플레이어 ASC는 PlayerState에 있어 Pawn보다 오래 살기 때문에 두 개념이 갈라집니다
	 */
	TWeakObjectPtr<UObject> CurrentSource;

	/** 피해 이벤트를 구독한 ASC입니다 */
	TWeakObjectPtr<URSAbilitySystemComponent> ConnectedAbilitySystemComp;

	/**
	 * 대상의 바닥에서 데미지 숫자를 띄울 높이(cm)이며 현재 플레이어 Pawn의 캡슐 전체 높이입니다
	 * 플레이어의 월드 Z가 아니라 체형만 사용하므로 점프와 경사에 따라 표시 높이가 흔들리지 않습니다
	 */
	float DamageNumberStartHeight = 0.0f;
};
