// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "RSDamageNumberTestTypes.generated.h"

class URSDamageNumberViewModel;

/**
 * URSDamageNumberViewModel의 요청 델리게이트를 수집하는 자동화 테스트 전용 객체입니다
 * 동적 델리게이트는 람다로 구독할 수 없어 UFUNCTION 핸들러를 가진 UObject가 필요합니다
 * UHT가 처리하는 헤더에 있어야 하므로 WITH_DEV_AUTOMATION_TESTS로 감싸지 않습니다
 */
UCLASS()
class URSDamageNumberTestListener : public UObject
{
	GENERATED_BODY()

public:
	/** 전달된 ViewModel의 요청과 폐기 이벤트를 구독합니다 */
	void ListenTo(URSDamageNumberViewModel* InViewModel);

	/** 수집한 요청과 폐기 기록을 모두 지웁니다 */
	void ClearRecords();

private:
	/** 표시 요청을 기록합니다 */
	UFUNCTION()
	void HandleDamageNumberRequested(FText DisplayDamageText, FVector WorldAnchor);

	/** 폐기 요청을 기록합니다 */
	UFUNCTION()
	void HandleDamageNumberResetRequested();

public:
	/** 수집한 표시 요청의 표시 문자열입니다 */
	TArray<FString> RequestedTexts;

	/** 수집한 표시 요청의 월드 앵커입니다 */
	TArray<FVector> RequestedAnchors;

	/** 수집한 폐기 요청의 횟수입니다 */
	int32 ResetCount = 0;
};
