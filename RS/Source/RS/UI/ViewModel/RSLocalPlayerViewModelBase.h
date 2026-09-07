// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "RSLocalPlayerViewModelBase.generated.h"

/** LocalPlayer 단위 저장소가 전달하는 게임 데이터 원본을 선택적으로 관찰하는 ViewModel 기반 클래스입니다 */
UCLASS(Abstract)
class RS_API URSLocalPlayerViewModelBase : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/** 새로운 데이터 원본이 등록되었을 때 필요한 타입이면 연결합니다 */
	virtual void HandleSourceRegistered(UObject* Source);

	/** 데이터 원본이 해제되었을 때 현재 연결한 원본이면 정리합니다 */
	virtual void HandleSourceUnregistered(UObject* Source);
};
