// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "RSViewModelWidget.h"
#include "RSAbilitySlotWidget.generated.h"

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

private:
	/** 이 슬롯이 담당하는 입력 자리이며 배치한 인스턴스마다 지정합니다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RS|Ability Slot", meta = (AllowPrivateAccess = "true", Categories = "InputTag.Ability"))
	FGameplayTag InputTag;

};
