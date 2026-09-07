// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RSAbilityDefinition.generated.h"

/**
 * Ability를 사용자 인터페이스에 표시할 때 사용하는 정적 데이터를 제공합니다
 * 실행 규칙, 쿨다운과 Ability 식별 태그는 URSBaseGameplayAbility가 소유하며 이 애셋은 표시 정보만 담당합니다
 */
UCLASS(BlueprintType)
class RS_API URSAbilityDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

#if WITH_EDITOR

public:
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;

#endif

public:
	/** 슬롯과 툴팁에 표시할 이름입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Presentation")
	FText DisplayName;

	/** 슬롯에 표시할 아이콘이며 실제 로드 시점은 사용자 인터페이스 계층이 결정합니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Presentation")
	TSoftObjectPtr<UTexture2D> Icon;

	/** 툴팁 등에 표시할 설명이며 현재 표시하는 위젯은 없습니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Presentation", meta = (MultiLine = true))
	FText Description;
};
