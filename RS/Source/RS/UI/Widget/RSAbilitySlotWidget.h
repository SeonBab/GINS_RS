// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "RSViewModelWidget.h"
#include "RSAbilitySlotWidget.generated.h"

class UMaterialInterface;

/**
 * 어빌리티 슬롯 하나를 표시하는 위젯입니다
 * 슬롯 목록은 데이터가 아니라 배치이므로, 이 위젯을 Ability Bar에 필요한 수만큼 두고 인스턴스마다 입력 자리를 지정합니다
 */
UCLASS(Abstract)
class RS_API URSAbilitySlotWidget : public URSViewModelWidget
{
	GENERATED_BODY()

public:
	/** 이 슬롯이 담당하는 입력 자리를 반환합니다 */
	const FGameplayTag& GetInputTag() const { return InputTag; }

	/** 아이콘을 그릴 때 사용할 머티리얼을 반환하며 지정하지 않았으면 nullptr입니다 */
	UMaterialInterface* GetIconMaterial() const { return IconMaterial; }

private:
	/** 이 슬롯이 담당하는 입력 자리이며 배치한 인스턴스마다 지정합니다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RS|Ability Slot", meta = (AllowPrivateAccess = "true", Categories = "InputTag.Ability"))
	FGameplayTag InputTag;

	/**
	 * 아이콘을 그릴 때 사용할 머티리얼입니다
	 * 쿨다운 중 채도를 낮추려면 IconTexture와 Desaturation 파라미터를 가진 머티리얼을 지정합니다
	 * 비워 두면 아이콘 텍스처를 그대로 그립니다
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Ability Slot", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMaterialInterface> IconMaterial;
};
