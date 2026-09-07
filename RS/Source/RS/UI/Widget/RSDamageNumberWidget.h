// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RSDamageNumberWidget.generated.h"

class UTextBlock;

/**
 * 데미지 숫자 하나를 표시하는 위젯입니다
 * 자기 위치와 수명은 관리하지 않으며 계층 위젯이 풀에서 재사용합니다
 */
UCLASS(Abstract)
class RS_API URSDamageNumberWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 표시할 피해 문자열을 갱신합니다 */
	void SetDamageText(const FText& InDamageText);

private:
	/** 피해 문자열을 표시하는 텍스트입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "RS|Damage Number", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> TextBlock_Damage;
};
