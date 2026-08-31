// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RSViewModelWidget.generated.h"

class UMVVMViewModelBase;

/** ViewModel을 클래스 기준으로 설정할 수 있는 사용자 인터페이스 위젯입니다 */
UCLASS(Abstract)
class RS_API URSViewModelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 위젯에 선언된 호환 가능한 수동 ViewModel 소스에 인스턴스를 설정합니다 */
	UFUNCTION(BlueprintCallable, Category = "RS|ViewModel")
	bool SetViewModel(UMVVMViewModelBase* InViewModel);
};
