// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RSBossPhaseComponent.h"
#include "RSBossPhaseData.generated.h"

class URSBaseGameplayAbility;

/**
 * 보스 페이즈 하나가 사용하는 패턴 후보와 진행 규칙입니다
 * 페이즈마다 다른 것은 후보 목록, 사이클 순서, 체력 트리거와 메인 기믹뿐입니다
 */
USTRUCT(BlueprintType)
struct FRSBossPhaseDefinition
{
	GENERATED_BODY()

	/** 상시 차례에 사용할 패턴 후보이며 이 중 하나를 무작위로 선택합니다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RS|Boss|Pattern")
	TArray<TSubclassOf<URSBaseGameplayAbility>> BasicPatterns;

	/** 특수 차례에 사용할 패턴 후보이며 이 중 하나를 무작위로 선택합니다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RS|Boss|Pattern")
	TArray<TSubclassOf<URSBaseGameplayAbility>> SpecialPatterns;

	/**
	 * 패턴 종류를 어떤 순서로 반복할지 정의합니다
	 * 순서만 다른 페이즈는 Behavior Tree를 바꾸지 않고 이 배열로 표현합니다
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RS|Boss|Pattern")
	TArray<ERSBossPatternType> CycleSequence = { ERSBossPatternType::Basic, ERSBossPatternType::Basic, ERSBossPatternType::Special };

	/**
	 * 이 페이즈를 끝내고 다음 페이즈로 넘어가는 체력 기준이며 최대 체력에 대한 비율입니다
	 * 배열의 마지막 페이즈는 넘어갈 다음 페이즈가 없으므로 이 값을 사용하지 않습니다
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RS|Boss|Phase", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float HealthTriggerRatio = 0.0f;

	/**
	 * 페이즈 전환 전에 실행할 메인 기믹이며 선택 사항입니다
	 * 비어 있으면 기믹 없이 체력 기준만으로 다음 페이즈로 넘어갑니다
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RS|Boss|Phase")
	TSubclassOf<URSBaseGameplayAbility> MainGimmickAbility;
};

/**
 * 보스 한 마리의 페이즈 진행 데이터입니다
 * 페이즈 개수 변경이 코드 수정 없이 이 애셋의 배열 편집으로 끝나도록 구성합니다
 */
UCLASS(BlueprintType, Const)
class RS_API URSBossPhaseData : public UDataAsset
{
	GENERATED_BODY()

public:
	/** 진행 순서대로 나열한 페이즈 정의입니다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RS|Boss|Phase")
	TArray<FRSBossPhaseDefinition> Phases;

#if WITH_EDITOR
	/** 사이클에 쓰는 패턴 종류마다 후보가 있는지 등 페이즈 데이터의 불변 조건을 검사합니다 */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
