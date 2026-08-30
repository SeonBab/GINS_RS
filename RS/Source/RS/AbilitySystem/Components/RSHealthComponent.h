// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RSHealthComponent.generated.h"

class URSAbilitySystemComponent;
class URSHealthComponent;
class URSHealthSet;

struct FOnAttributeChangeData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FRSHealthChangedSignature, URSHealthComponent*, HealthComponent, float, OldValue, float, NewValue);

/**
 * HealthSet의 체력 변경을 캐릭터, UI 등의 외부 시스템에 전달합니다
 * 실제 체력 데이터는 소유하지 않고 ASC와 HealthSet을 연결하는 역할을 합니다
 */
UCLASS(ClassGroup = "RS", meta = (BlueprintSpawnableComponent))
class RS_API URSHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URSHealthComponent();

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	/** 사용할 ASC와 HealthSet을 연결하고 체력 Attribute 변경 이벤트를 구독합니다 */
	void InitializeWithAbilitySystem(URSAbilitySystemComponent* InAbilitySystemComponent);

	/** 기존 Attribute 변경 이벤트를 해제하고 참조를 초기화합니다 */
	void UninitializeFromAbilitySystem();

	/** 현재 체력을 반환합니다 */
	UFUNCTION(BlueprintPure, Category = "RS|Health")
	float GetHealth() const;

	/** 최대 체력을 반환합니다 */
	UFUNCTION(BlueprintPure, Category = "RS|Health")
	float GetMaxHealth() const;

	/** 0~1 범위로 정규화한 체력 비율을 반환합니다 */
	UFUNCTION(BlueprintPure, Category = "RS|Health")
	float GetHealthNormalized() const;

public:
	/** 현재 체력이 변경될 때 발생합니다 */
	UPROPERTY(BlueprintAssignable, Category = "RS|Health")
	FRSHealthChangedSignature OnHealthChanged;

	/** 최대 체력이 변경될 때 발생합니다 */
	UPROPERTY(BlueprintAssignable, Category = "RS|Health")
	FRSHealthChangedSignature OnMaxHealthChanged;

private:
	/** ASC에서 전달된 현재 체력 변경을 외부 이벤트로 변환합니다 */
	void HandleHealthChanged(const FOnAttributeChangeData& ChangeData);

	/** ASC에서 전달된 최대 체력 변경을 외부 이벤트로 변환합니다 */
	void HandleMaxHealthChanged(const FOnAttributeChangeData& ChangeData);

private:
	/** 현재 체력 Attribute를 제공하는 ASC입니다 */
	UPROPERTY(Transient)
	TObjectPtr<URSAbilitySystemComponent> AbilitySystemComp;

	/** ASC에 등록된 체력 AttributeSet입니다 */
	UPROPERTY(Transient)
	TObjectPtr<const URSHealthSet> HealthSet;

	/** ASC에서 현재 체력 변경 이벤트를 제거하기 위한 핸들입니다 */
	FDelegateHandle HealthChangedDelegateHandle;

	/** ASC에서 최대 체력 변경 이벤트를 제거하기 위한 핸들입니다 */
	FDelegateHandle MaxHealthChangedDelegateHandle;
};
