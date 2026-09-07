// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RSAbilitySlotViewModel.h"
#include "RSAbilityBarWidget.generated.h"

/**
 * 배치된 어빌리티 슬롯 위젯에 각자의 ViewModel을 전달합니다
 * 슬롯 목록을 따로 소유하지 않고 배치된 슬롯 위젯이 자신의 입력 자리를 알려주므로, 슬롯을 추가하려면 위젯을 배치하기만 하면 됩니다
 */
UCLASS(Abstract)
class RS_API URSAbilityBarWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	/** 위젯 계층이 준비되면 배치된 슬롯에 ViewModel을 연결합니다 */
	virtual void NativeOnInitialized() override;

private:
	/** 배치된 슬롯 위젯을 순회하며 각자의 입력 자리에 해당하는 ViewModel을 전달합니다 */
	void InitializeSlotViewModels();

private:
	/**
	 * 아이콘 머티리얼과 준비 완료 연출 시간처럼 슬롯을 그리는 방식을 정하는 설정입니다
	 * 슬롯마다 달라질 값이 아니므로 배치된 슬롯 위젯이 아니라 바가 소유하여 한 곳에서만 지정합니다
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Ability Bar", meta = (AllowPrivateAccess = "true"))
	FRSAbilitySlotPresentationConfig SlotPresentationConfig;
};
