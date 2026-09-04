// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "MVVMViewModelBase.h"
#include "Styling/SlateBrush.h"
#include "Tickable.h"
#include "RSAbilitySlotViewModel.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class URSAbilityDefinition;

/**
 * 어빌리티 슬롯 하나가 표시할 값을 제공합니다
 * 게임 객체를 관찰하지 않고 URSPlayerAbilityViewModel이 밀어 넣은 값만 보관하므로 LocalPlayer 저장소에 등록하지 않습니다
 * 쿨다운 중에는 남은 시간과 진행률을 스스로 갱신하여 위젯이 계산 없이 바인딩만으로 표시할 수 있게 합니다
 */
UCLASS(BlueprintType)
class RS_API URSAbilitySlotViewModel : public UMVVMViewModelBase, public FTickableGameObject
{
	GENERATED_BODY()

#pragma region Tick

public:
	/** 쿨다운이 진행되는 동안 남은 시간과 진행률을 갱신합니다 */
	virtual void Tick(float DeltaTime) override;

	/** 쿨다운 중일 때만 Tick하여 표시할 것이 없는 슬롯이 매 프레임 비용을 만들지 않게 합니다 */
	virtual bool IsTickable() const override;

	virtual TStatId GetStatId() const override;

	/** 소유한 LocalPlayer의 월드를 따라 Tick하도록 월드를 반환합니다 */
	virtual UWorld* GetTickableGameObjectWorld() const override;

#pragma endregion

public:
	/** 이 슬롯이 담당하는 입력 자리를 반환합니다 */
	const FGameplayTag& GetInputTag() const;

	/** 이 슬롯이 담당할 입력 자리를 설정합니다 */
	void SetInputTag(const FGameplayTag& InInputTag);

	/**
	 * 아이콘을 그릴 때 사용할 머티리얼을 지정합니다
	 * 쿨다운 중 채도를 낮추려면 필요하며, 지정하지 않으면 텍스처를 그대로 사용합니다
	 */
	void SetIconMaterial(UMaterialInterface* InIconMaterial);

	/** 슬롯에 표시할 키 텍스트를 설정합니다 */
	void SetInputKeyText(const FText& InInputKeyText);

	/** 현재 슬롯을 대표하는 어빌리티의 표시 정보를 반영하며 nullptr이면 빈 슬롯으로 만듭니다 */
	void SetPresentation(const URSAbilityDefinition* Definition, UTexture2D* InIconTexture);

	/** 쿨다운 시작을 반영하고 갱신을 시작합니다 */
	void SetCooldown(float InCooldownEndTime, float InCooldownDuration);

	/** 쿨다운 종료를 반영하고 갱신을 멈춥니다 */
	void ClearCooldown();

private:
	/** 현재 월드 시간을 기준으로 남은 시간과 진행률을 다시 계산합니다 */
	void UpdateCooldownValues();

	/** 쿨다운 여부에 따라 아이콘의 채도를 갱신합니다 */
	void UpdateIconDesaturation();

private:
	/** 슬롯이 담당하는 입력 자리이며 표시 값이 아니므로 변경을 통지하지 않습니다 */
	FGameplayTag InputTag;

	/** 아이콘을 그릴 머티리얼이며 슬롯 위젯이 지정합니다 */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> IconMaterial;

	/**
	 * 어빌리티마다 다른 텍스처와 슬롯마다 다른 채도를 적용하기 위한 실행별 머티리얼입니다
	 * 값만 바꾸면 되므로 Brush를 다시 만들지 않고 변경 통지도 발생하지 않습니다
	 */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> IconMaterialInstance;

	/** 현재 슬롯을 대표하는 어빌리티의 이름입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, FieldNotify, Category = "RS|Ability Slot", meta = (AllowPrivateAccess = "true"))
	FText DisplayName;

	/**
	 * 현재 슬롯을 대표하는 어빌리티의 아이콘입니다
	 * Image의 Brush에 변환 함수 없이 바로 바인딩할 수 있도록 Brush 형태로 제공합니다
	 */
	UPROPERTY(Transient, BlueprintReadOnly, FieldNotify, Category = "RS|Ability Slot", meta = (AllowPrivateAccess = "true"))
	FSlateBrush Icon;

	/** 이 슬롯에 현재 연결된 입력 키의 표시 문자열입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, FieldNotify, Category = "RS|Ability Slot", meta = (AllowPrivateAccess = "true"))
	FText InputKeyText;

	/** 표시할 어빌리티가 있는지 나타내며 슬롯을 비활성 표현으로 전환할 때 사용합니다 */
	UPROPERTY(Transient, BlueprintReadOnly, FieldNotify, Category = "RS|Ability Slot", meta = (AllowPrivateAccess = "true"))
	bool bHasAbility = false;

	/** 현재 쿨다운이 진행 중인지 나타내며 Tick 여부도 이 값이 결정합니다 */
	UPROPERTY(Transient, BlueprintReadOnly, FieldNotify, Category = "RS|Ability Slot", meta = (AllowPrivateAccess = "true"))
	bool bIsOnCooldown = false;

	/** 쿨다운이 끝나기까지 남은 시간입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, FieldNotify, Category = "RS|Ability Slot", meta = (AllowPrivateAccess = "true"))
	float CooldownRemaining = 0.0f;

	/** 쿨다운의 남은 비율이며 1에서 0으로 줄어듭니다 */
	UPROPERTY(Transient, BlueprintReadOnly, FieldNotify, Category = "RS|Ability Slot", meta = (AllowPrivateAccess = "true"))
	float CooldownPercent = 0.0f;

	/** 쿨다운이 끝나는 월드 시간입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, FieldNotify, Category = "RS|Ability Slot", meta = (AllowPrivateAccess = "true"))
	float CooldownEndTime = 0.0f;

	/** 이번 쿨다운의 전체 시간입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, FieldNotify, Category = "RS|Ability Slot", meta = (AllowPrivateAccess = "true"))
	float CooldownDuration = 0.0f;
};
