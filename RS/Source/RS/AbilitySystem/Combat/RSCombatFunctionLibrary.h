// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ScalableFloat.h"
#include "RSCombatFunctionLibrary.generated.h"

/** 판정 범위의 형상 종류입니다 */
UENUM(BlueprintType)
enum class ERSCombatShapeType : uint8
{
	Box		UMETA(DisplayName = "Box",		ToolTip = "배치 회전을 따르는 직육면체입니다."),
	Sphere	UMETA(DisplayName = "Sphere",	ToolTip = "수평 거리로 판정하며 InnerRadius를 주면 도넛이 됩니다.")
};

/**
 * 판정 범위의 형상과 크기이며 위치와 회전은 담지 않습니다
 * 위치를 분리해야 같은 형상을 공격자 기준과 월드 고정처럼 다른 방식으로 배치할 수 있습니다
 */
USTRUCT(BlueprintType)
struct FRSCombatShape
{
	GENERATED_BODY()

	/** 이 판정이 사용할 형상입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Combat|Shape")
	ERSCombatShapeType Type = ERSCombatShapeType::Box;

	/** 판정 박스의 각 축 반크기입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Combat|Shape", meta = (EditCondition = "Type == ERSCombatShapeType::Box"))
	FVector BoxExtent = FVector(60.0f, 60.0f, 90.0f);

	/** 판정에 포함할 바깥 반지름입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Combat|Shape", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm", EditCondition = "Type == ERSCombatShapeType::Sphere"))
	float Radius = 100.0f;

	/** 판정에서 제외할 안쪽 반지름이며 0보다 크면 가운데가 비어 있는 도넛이 됩니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Combat|Shape", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm", EditCondition = "Type == ERSCombatShapeType::Sphere"))
	float InnerRadius = 0.0f;
};

/** 타격이 대상에게 요청하는 피격 반응의 종류입니다 */
UENUM(BlueprintType)
enum class ERSHitReactionType : uint8
{
	None		UMETA(DisplayName = "None",			ToolTip = "피해만 적용하고 반응을 요청하지 않습니다."),
	HitReact	UMETA(DisplayName = "Hit React",	ToolTip = "짧은 피격 경직을 요청합니다."),
	Knockdown	UMETA(DisplayName = "Knockdown",	ToolTip = "밀려나 넘어지는 넉다운을 요청합니다.")
};

/**
 * 넉다운 요청이 대상에게 전달할 밀려나는 거리, 뜨는 높이와 시간입니다
 * FGameplayEventData에는 float이 하나뿐이라 세 값을 TargetData로 전달합니다
 */
USTRUCT()
struct FRSKnockbackTargetData : public FGameplayAbilityTargetData
{
	GENERATED_BODY()

	/** 수평으로 밀려날 거리입니다 */
	UPROPERTY()
	float Distance = 0.0f;

	/** 뜨는 것처럼 보이게 할 정점 높이이며 0이면 뜨지 않습니다 */
	UPROPERTY()
	float Height = 0.0f;

	/** 밀려나고 뜨는 데 걸릴 시간입니다 */
	UPROPERTY()
	float Duration = 0.0f;

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FRSKnockbackTargetData::StaticStruct();
	}

	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
	{
		Ar << Distance;
		Ar << Height;
		Ar << Duration;

		bOutSuccess = true;

		return true;
	}
};

template<>
struct TStructOpsTypeTraits<FRSKnockbackTargetData> : public TStructOpsTypeTraitsBase2<FRSKnockbackTargetData>
{
	enum
	{
		WithNetSerializer = true
	};
};

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

	/** 이 타격이 대상에게 요청할 피격 반응입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Combat|Reaction")
	ERSHitReactionType Reaction = ERSHitReactionType::None;

	/** 넉다운이 대상을 수평으로 밀어낼 거리입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Combat|Reaction", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm", EditCondition = "Reaction == ERSHitReactionType::Knockdown"))
	float KnockbackDistance = 400.0f;

	/** 넉다운이 대상을 뜨는 것처럼 보이게 할 정점 높이이며 0이면 뜨지 않습니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Combat|Reaction", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm", EditCondition = "Reaction == ERSHitReactionType::Knockdown"))
	float KnockbackHeight = 80.0f;

	/** 넉다운의 밀려남과 뜸이 진행될 시간이며 거리·높이와 함께 속도를 결정합니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Combat|Reaction", meta = (ClampMin = "0.01", UIMin = "0.01", ForceUnits = "s", EditCondition = "Reaction == ERSHitReactionType::Knockdown"))
	float KnockbackDuration = 0.5f;
};

/** 여러 Ability가 상속 없이 공유하는 전투 판정 조회 기능을 제공합니다 */
UCLASS()
class RS_API URSCombatFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 주어진 월드 위치에 놓인 형상과 겹치는 유효한 판정 대상을 수집합니다
	 * 진영은 TargetChannel이 구분하므로 아군과 공격자 자신은 쿼리 단계에서 제외됩니다
	 * 크기는 Shape가 소유하므로 ShapeTransform의 Scale은 사용하지 않고, Sphere는 회전도 사용하지 않습니다
	 */
	UFUNCTION(BlueprintCallable, Category = "RS|Combat")
	static void FindTargetsInShape(const AActor* Attacker, ECollisionChannel TargetChannel, const FRSCombatShape& Shape, const FTransform& ShapeTransform, TArray<AActor*>& OutTargets);

	/** 판정 형상 드로우와 판정 결과 로그가 켜져 있는지 반환합니다 */
	static bool IsHitCheckDebugEnabled();
};
