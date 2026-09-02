// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ScalableFloat.h"
#include "RSCombatFunctionLibrary.generated.h"

/**
 * 한 번의 공격 판정이 사용할 범위와 피해량입니다
 * 하나의 공격이 여러 타격을 가질 수 있으므로 Ability는 이 정의를 배열로 소유하고 타격 순서대로 소비합니다
 */
USTRUCT(BlueprintType)
struct FRSHitCheckDefinition
{
	GENERATED_BODY()

	/** 판정 박스의 각 축 반크기입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Combat")
	FVector BoxExtent = FVector(60.0f, 60.0f, 90.0f);

	/** 판정 박스를 공격자 전방으로 밀어낼 거리입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Combat", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ForwardOffset = 80.0f;

	/** 이 타격이 대상에게 가할 피해량입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Combat")
	FScalableFloat Damage = 10.0f;
};

/** 여러 Ability가 상속 없이 공유하는 전투 판정 조회 기능을 제공합니다 */
UCLASS()
class RS_API URSCombatFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 공격자 전방의 회전된 박스와 겹치는 유효한 판정 대상을 수집합니다
	 * 진영은 TargetChannel이 구분하므로 아군과 공격자 자신은 쿼리 단계에서 제외됩니다
	 */
	UFUNCTION(BlueprintCallable, Category = "RS|Combat")
	static void FindTargetsInBox(const AActor* Attacker, ECollisionChannel TargetChannel, const FVector& BoxExtent, float ForwardOffset, TArray<AActor*>& OutTargets);

	/** 판정 박스 드로우와 판정 결과 로그가 켜져 있는지 반환합니다 */
	static bool IsHitCheckDebugEnabled();
};
