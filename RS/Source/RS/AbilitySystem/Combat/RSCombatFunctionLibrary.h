// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ScalableFloat.h"
#include "RSCombatFunctionLibrary.generated.h"

#if WITH_EDITOR
class FDataValidationContext;
#endif

/** 판정 범위의 형상 종류입니다 */
UENUM(BlueprintType)
enum class ERSCombatShapeType : uint8
{
	Box		UMETA(DisplayName = "Box",		ToolTip = "배치 회전을 따르는 직육면체입니다."),
	Sphere	UMETA(DisplayName = "Sphere",	ToolTip = "수평 거리로 판정하며 InnerRadius를 주면 도넛이 됩니다."),
	Cone	UMETA(DisplayName = "Cone",		ToolTip = "Transform Forward를 중심으로 하는 수평 부채꼴입니다.")
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

	/** Cone 꼭짓점에서 외곽까지의 수평 거리입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Combat|Shape", meta = (ClampMin = "0.0", UIMin = "1.0", ForceUnits = "cm", EditCondition = "Type == ERSCombatShapeType::Cone"))
	float Range = 500.0f;

	/** Cone의 양쪽 경계 사이 전체 각도입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Combat|Shape", meta = (ClampMin = "0.0", ClampMax = "180.0", UIMin = "1.0", UIMax = "180.0", ForceUnits = "deg", EditCondition = "Type == ERSCombatShapeType::Cone"))
	float Angle = 90.0f;

	/** Shape 종류별 데이터가 판정 가능한 범위인지 검사합니다 */
	bool IsDataValid(FString* OutValidationError = nullptr) const;
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
 * 타격이 대상에게 요청할 반응의 종류와 세기입니다
 * 공격마다 이 값을 따로 선언하지 않도록 한 곳에서만 정의하고 소비자가 하나씩 소유합니다
 */
USTRUCT(BlueprintType)
struct FRSHitReactionDefinition
{
	GENERATED_BODY()

	/** 대상에게 요청할 피격 반응입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Combat|Reaction")
	ERSHitReactionType Type = ERSHitReactionType::None;

	/** 넉다운이 대상을 수평으로 밀어낼 거리입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Combat|Reaction", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm", EditCondition = "Type == ERSHitReactionType::Knockdown"))
	float KnockbackDistance = 400.0f;

	/** 넉다운이 대상을 뜨는 것처럼 보이게 할 정점 높이이며 0이면 뜨지 않습니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Combat|Reaction", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm", EditCondition = "Type == ERSHitReactionType::Knockdown"))
	float KnockbackHeight = 80.0f;

	/** 넉다운의 밀려남과 뜸이 진행될 시간이며 거리·높이와 함께 속도를 결정합니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Combat|Reaction", meta = (ClampMin = "0.01", UIMin = "0.01", ForceUnits = "s", EditCondition = "Type == ERSHitReactionType::Knockdown"))
	float KnockbackDuration = 0.5f;
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
	FRSHitReactionDefinition Reaction;
};

#if !UE_BUILD_SHIPPING
/**
 * 전투 판정 확인용 콘솔 명령의 등록과 해제이며 RS 모듈이 시작하고 끝날 때 한 번씩 호출합니다
 * 전역 객체가 아니라 모듈 수명에 맞춰야 핫 리로드로 모듈을 다시 올려도 이전 등록이 남지 않습니다
 */
namespace RSCombatDebug
{
	void RegisterConsoleCommands();
	void UnregisterConsoleCommands();
}
#endif

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

	/** 대상 위치의 중심점이 Cone의 수평 범위 안에 있는지 검사합니다 */
	static bool IsLocationInsideCone(const FRSCombatShape& Shape, const FTransform& ShapeTransform, const FVector& TargetLocation);

	/**
	 * 대상 하나에게 이번 타격이 요청하는 피격 반응을 전달합니다
	 * 요청을 실제로 적용할지는 대상의 면역 태그와 반응 Ability가 결정합니다
	 */
	UFUNCTION(BlueprintCallable, Category = "RS|Combat")
	static void SendHitReaction(const AActor* Instigator, AActor* TargetActor, const FRSHitReactionDefinition& ReactionDefinition);

	/** 판정 형상 드로우와 판정 결과 로그가 켜져 있는지 반환합니다 */
	static bool IsHitCheckDebugEnabled();

	/**
	 * 판정 형상을 디버그로 그립니다
	 * 판정이 없는 예고 단계도 같은 형상 데이터로 같은 그림을 그리도록 그리기 코드를 한곳에 둡니다
	 */
	static void DrawDebugCombatShape(const UWorld* World, const FRSCombatShape& Shape, const FTransform& ShapeTransform, const FColor& Color, float LifeTime);

#if WITH_EDITOR

public:
	/**
	 * 데미지 값이 정수인지 확인하고 아니면 DamageLabel을 붙인 오류를 Context에 추가합니다
	 * RS의 데미지는 논리적으로 정수 단위이며 FScalableFloat가 float인 것은 GAS의 표현일 뿐입니다
	 * 커브 테이블을 참조하는 값은 모든 레벨을 확인할 수 없으므로 레벨 1의 평가값만 보는 부분 검증입니다
	 */
	static bool ValidateIntegerDamage(const FScalableFloat& Damage, const FString& DamageLabel, FDataValidationContext& Context);

public:
	/**
	 * 정수 데미지 검증에서 미세한 부동소수점 오차와 의도적인 소수 입력을 구분하는 도메인 tolerance입니다
	 * 수치 계산의 정밀도를 보장하는 값이 아니므로 KINDA_SMALL_NUMBER보다 넉넉하게 둡니다
	 */
	static constexpr float DamageIntegerTolerance = 0.01f;

#endif
};
