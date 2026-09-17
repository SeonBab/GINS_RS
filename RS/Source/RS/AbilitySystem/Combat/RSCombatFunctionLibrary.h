// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RSNiagaraSpawnDefinition.h"
#include "ScalableFloat.h"
#include "RSCombatFunctionLibrary.generated.h"

class UCameraShakeBase;
class UNiagaraComponent;
class UNiagaraSystem;
class USkeletalMeshComponent;
class USoundBase;

#if WITH_EDITOR
class FDataValidationContext;
#endif

/**
 * 수평 환형 부채꼴의 경계이며 위치와 회전은 담지 않습니다
 * 안쪽·바깥 반지름과 시작 각도, 부호 있는 각도 폭으로 꽉 찬 원, 도넛과 부채꼴을 모두 표현합니다
 * SweepAngleDegrees의 부호가 펼쳐지는 방향이며 절댓값이 360도면 모든 각도를 덮습니다
 */
struct FRSAnnularSectorBounds
{
	/** 판정에서 제외할 안쪽 반지름이며 0이면 중심까지 채웁니다 */
	float InnerRadius = 0.0f;

	/** 판정에 포함할 바깥 반지름이며 InnerRadius보다 커야 합니다 */
	float OuterRadius = 0.0f;

	/** 배치 Transform의 Forward에서 첫 경계까지의 상대 각도입니다 */
	float StartYawOffset = 0.0f;

	/** 첫 경계에서 펼쳐질 각도이며 부호가 방향을 결정합니다 */
	float SweepAngleDegrees = 0.0f;

	/** 판정 가능한 경계인지 검사하며 형상과 판정 커널이 이 규칙 하나를 공유합니다 */
	bool IsDataValid(FString* OutValidationError = nullptr) const;

	/** 절댓값이 360도라 각도 검사가 의미를 갖지 않는지 반환합니다 */
	bool CoversEveryAngle() const;
};

/** 판정 범위의 형상 종류입니다 */
UENUM(BlueprintType)
enum class ERSCombatShapeType : uint8
{
	Box				UMETA(DisplayName = "Box",				ToolTip = "배치 회전을 따르는 직육면체입니다."),
	AnnularSector	UMETA(DisplayName = "Annular Sector",	ToolTip = "수평 환형 부채꼴입니다. Sweep이 360도면 꽉 찬 원이나 도넛이 되고 그보다 작으면 부채꼴이 됩니다.")
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

	/**
	 * 판정에서 제외할 안쪽 반지름이며 0보다 크면 가운데가 비어 있는 도넛이 됩니다
	 * 런타임이 이 값을 바꾸지 않으므로 에셋에 적은 값이 곧 실제 안쪽 경계이며 0은 형상 원점부터 판정한다는 뜻입니다
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Combat|Shape", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm", EditCondition = "Type == ERSCombatShapeType::AnnularSector"))
	float InnerRadius = 0.0f;

	/** 판정에 포함할 바깥 반지름이며 기본값 0은 아직 정하지 않았다는 뜻이라 검증에서 걸립니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Combat|Shape", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm", EditCondition = "Type == ERSCombatShapeType::AnnularSector"))
	float OuterRadius = 0.0f;

	/** 배치 Transform의 Forward에서 부채꼴의 첫 경계까지의 상대 각도입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Combat|Shape", meta = (ForceUnits = "deg", EditCondition = "Type == ERSCombatShapeType::AnnularSector"))
	float StartYawOffset = 0.0f;

	/** 첫 경계에서 펼쳐질 각도이며 부호가 방향을 결정하고 절댓값이 360도면 모든 각도를 덮습니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Combat|Shape", meta = (ClampMin = "-360.0", ClampMax = "360.0", ForceUnits = "deg", EditCondition = "Type == ERSCombatShapeType::AnnularSector"))
	float SweepAngleDegrees = 360.0f;

	/** 환형 부채꼴 계산이 읽을 경계를 반환하며 Box에서는 의미를 갖지 않습니다 */
	FRSAnnularSectorBounds GetAnnularSectorBounds() const;

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
 * 넉다운 요청이 대상에게 전달할 방향, 밀려나는 거리, 뜨는 높이와 시간입니다
 * FGameplayEventData에는 필요한 값을 모두 담을 수 없어 TargetData로 전달합니다
 */
USTRUCT()
struct FRSKnockbackTargetData : public FGameplayAbilityTargetData
{
	GENERATED_BODY()

	/** 공격이 기존 공격자 반대 방향 대신 사용할 방향을 지정했는지 나타냅니다 */
	UPROPERTY()
	bool bHasKnockbackDirection = false;

	/** 공격이 지정한 월드 수평 방향이며 소비할 때 Z를 제거하고 정규화합니다 */
	UPROPERTY()
	FVector KnockbackDirection = FVector::ZeroVector;

	/** 수평으로 밀려날 거리입니다 */
	UPROPERTY()
	float Distance = 0.0f;

	/** 뜨는 것처럼 보이게 할 정점 높이이며 0이면 뜨지 않습니다 */
	UPROPERTY()
	float Height = 0.0f;

	/** 밀려나고 뜨는 데 걸릴 시간입니다 */
	UPROPERTY()
	float Duration = 0.0f;

	/** 명시된 방향을 수평 단위 벡터로 반환하며 미지정 또는 잘못된 방향이면 실패합니다 */
	bool TryGetNormalizedDirection(FVector& OutDirection) const
	{
		OutDirection = FVector::ZeroVector;
		if (!bHasKnockbackDirection || KnockbackDirection.ContainsNaN())
		{
			return false;
		}

		OutDirection = KnockbackDirection;
		OutDirection.Z = 0.0f;

		return OutDirection.Normalize();
	}

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FRSKnockbackTargetData::StaticStruct();
	}

	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
	{
		Ar.SerializeBits(&bHasKnockbackDirection, 1);
		if (bHasKnockbackDirection)
		{
			Ar << KnockbackDirection;
		}
		else if (Ar.IsLoading())
		{
			KnockbackDirection = FVector::ZeroVector;
		}

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
 * 보는 사람의 카메라를 한 번 흔들 요청이며 에셋을 비우면 흔들지 않습니다
 * 적중을 기준으로 삼는 플레이어 피드백과 판정을 기준으로 삼는 보스 패턴이 같은 값 구성을 공유하도록 분리했습니다
 */
USTRUCT(BlueprintType)
struct FRSCameraShakeDefinition
{
	GENERATED_BODY()

	/** 보는 사람의 카메라를 흔들 Camera Shake입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Combat|Feedback")
	TSubclassOf<UCameraShakeBase> ShakeClass;

	/** 셰이크 에셋의 진폭에 곱할 배율이며 같은 에셋으로 타격마다 세기를 다르게 합니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Combat|Feedback", meta = (ClampMin = "0.0", UIMin = "0.0", EditCondition = "ShakeClass != nullptr"))
	float Scale = 1.0f;
};

/** 카메라 셰이크를 언제 흔들지 정합니다 */
UENUM(BlueprintType)
enum class ERSCameraShakeTiming : uint8
{
	OnHit		UMETA(DisplayName = "On Hit",		ToolTip = "적중한 대상이 있을 때만 흔듭니다."),
	OnHitCheck	UMETA(DisplayName = "On Hit Check",	ToolTip = "적중 여부와 무관하게 판정하는 순간마다 흔듭니다.")
};

/**
 * 한 번의 타격이 만드는 연출이며 전부 선택적입니다
 * 반응과 마찬가지로 공격이 소유하므로 같은 공격의 1타와 2타에 다른 연출을 줄 수 있습니다
 */
USTRUCT(BlueprintType)
struct FRSHitFeedbackDefinition
{
	GENERATED_BODY()

	/** 적중한 대상마다 그 자리에서 재생할 Niagara입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Combat|Feedback")
	TObjectPtr<UNiagaraSystem> ImpactNiagara;

	/** 적중했을 때 첫 대상 위치에서 한 번 재생할 Sound입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Combat|Feedback")
	TObjectPtr<USoundBase> ImpactSound;

	/** 보는 사람의 카메라를 흔들 셰이크이며 재생 시점은 CameraShakeTiming이 정합니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Combat|Feedback")
	FRSCameraShakeDefinition CameraShake;

	/** 카메라 셰이크를 흔들 시점이며 플레이어 공격은 적중 시, 보스 패턴은 판정마다 흔듭니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Combat|Feedback")
	ERSCameraShakeTiming CameraShakeTiming = ERSCameraShakeTiming::OnHit;

	/** 적중했을 때 공격자를 멈칫하게 할 시간이며 0이면 히트스톱이 없습니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Combat|Feedback", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "s"))
	float HitStopDuration = 0.0f;

	/** 멈칫하는 동안 공격자에게 적용할 시간 배율이며 작을수록 더 강하게 멈춥니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Combat|Feedback", meta = (ClampMin = "0.01", ClampMax = "1.0", UIMin = "0.01", UIMax = "1.0", EditCondition = "HitStopDuration > 0.0"))
	float HitStopTimeDilation = 0.1f;
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

	/** 이 타격이 재생할 연출입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Combat|Feedback")
	FRSHitFeedbackDefinition Feedback;
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

	/** 복합 판정의 후보만 수집하고 중간 형상은 디버그로 그리지 않습니다 */
	static void FindTargetsInShapeWithoutDebugDraw(const AActor* Attacker, ECollisionChannel TargetChannel, const FRSCombatShape& Shape, const FTransform& ShapeTransform, TArray<AActor*>& OutTargets);

	/**
	 * 대상 위치의 수평 중심점이 환형 부채꼴 경계 안에 있는지 검사합니다
	 * 반지름은 [Inner, Outer), 각도는 [Start, Start + Sweep)의 반개구간이며 경계는 바깥쪽 형상의 것입니다
	 * 이 규약 덕분에 이어 붙는 링이나 조각이 경계를 공유해도 한 대상이 두 형상에 걸리거나 어느 쪽에서도 빠지지 않습니다
	 * 절댓값이 360도인 Sweep은 시작 경계와 끝 경계가 같은 지점이므로 각도 검사를 하지 않고 모두 포함합니다
	 * 판정할 수 없는 경계는 조용히 비우지 않고 거부하므로 잘못된 데이터가 적중 0건으로 보이지 않습니다
	 */
	static bool IsLocationInsideAnnularSector(const FTransform& SectorTransform, const FRSAnnularSectorBounds& SectorBounds, const FVector& TargetLocation);

	/**
	 * 패턴 공간의 기준이 될 액터의 캡슐 바닥 위치를 얻습니다
	 * Telegraph와 연출이 캡슐 중심 높이에 뜨지 않도록 모든 호출처가 같은 정의를 한 곳에서 읽습니다
	 * 캡슐이 없는 액터는 Simple Collision의 반높이로 대신하며, 액터가 없거나 결과가 유효하지 않으면 실패합니다
	 */
	static bool TryGetActorGroundLocation(const AActor* Actor, FVector& OutGroundLocation);

	/**
	 * 판정 형상 내부를 균일 격자로 채우는 월드 Transform 목록을 만듭니다
	 * 연출이 판정 범위를 벗어나지 않도록 셀 중심이 형상 안에 있는 칸만 남깁니다
	 * 각 Transform은 ShapeTransform의 회전과 높이를 그대로 상속하고 Scale은 사용하지 않습니다
	 * Spacing이 형상보다 넓어 격자가 한 칸으로 무너지면 형상을 대표하는 지점 하나를 채우므로 성공한 호출의 결과는 비지 않습니다
	 * 채우는 보스 패턴이 없는 Box와, 순회할 칸이 상한을 넘는 Spacing에서는 아무것도 채우지 않고 실패합니다
	 */
	static bool BuildShapeFillTransforms(const FRSCombatShape& Shape, const FTransform& ShapeTransform, float Spacing, TArray<FTransform>& OutTransforms);

	/**
	 * 대상 하나에게 이번 타격이 요청하는 피격 반응을 전달합니다
	 * 요청을 실제로 적용할지는 대상의 면역 태그와 반응 Ability가 결정합니다
	 */
	UFUNCTION(BlueprintCallable, Category = "RS|Combat")
	static void SendHitReaction(const AActor* Instigator, AActor* TargetActor, const FRSHitReactionDefinition& ReactionDefinition);

	/**
	 * 이번 타격의 연출을 재생하며 적중한 대상이 없어도 호출할 수 있습니다
	 * 보스 패턴의 카메라 셰이크는 빗나간 판정에서도 흔들려야 하므로 빈 대상 배열을 그대로 받습니다
	 * 항목마다 재생 횟수가 달라 대상 하나가 아니라 이번 판정에 걸린 대상 전체를 받습니다
	 */
	UFUNCTION(BlueprintCallable, Category = "RS|Combat")
	static void PlayHitFeedback(AActor* Attacker, const TArray<AActor*>& HitTargets, const FRSHitFeedbackDefinition& Feedback);

	/**
	 * 정의에 셰이크 에셋이 있으면 보는 사람의 카메라를 흔듭니다
	 * 공격자가 아니라 로컬 플레이어의 컨트롤러로 보내므로 AI가 소유한 보스 패턴도 같은 경로를 사용합니다
	 */
	UFUNCTION(BlueprintCallable, Category = "RS|Combat", meta = (WorldContext = "WorldContextObject"))
	static void PlayCameraShake(const UObject* WorldContextObject, const FRSCameraShakeDefinition& ShakeDefinition);

	/** 이번 판정에서 카메라를 흔들어야 하는지 시점 설정과 적중 여부로 판단합니다 */
	static bool ShouldPlayCameraShake(const FRSHitFeedbackDefinition& Feedback, bool bHasHitTargets);

	/** 지정한 월드 방향을 사용하는 넉백 반응을 대상에게 요청합니다 */
	static void SendHitReactionWithKnockbackDirection(const AActor* Instigator, AActor* TargetActor, const FRSHitReactionDefinition& ReactionDefinition, const FVector& KnockbackDirection);

	/**
	 * Niagara 정의가 요청한 기준으로 System을 생성합니다
	 * 소켓 기준 생성은 호출자가 소유한 Mesh를 사용하므로 어빌리티와 액터 어느 쪽이 부르든 같은 규칙을 따릅니다
	 * 정의가 비었거나 요청한 소켓이 없으면 아무것도 만들지 않고 nullptr을 반환합니다
	 */
	static UNiagaraComponent* SpawnNiagaraFromDefinition(const UObject* WorldContextObject, USkeletalMeshComponent* MeshComponent, const FRSNiagaraSpawnDefinition& Definition, const FTransform& WorldTransform, bool bAutoDestroy);

	/** 판정 형상 드로우와 판정 결과 로그가 켜져 있는지 반환합니다 */
	static bool IsHitCheckDebugEnabled();

	/**
	 * 판정 형상을 디버그로 그립니다
	 * 판정이 없는 예고 단계도 같은 형상 데이터로 같은 그림을 그리도록 그리기 코드를 한곳에 둡니다
	 */
	static void DrawDebugCombatShape(const UWorld* World, const FRSCombatShape& Shape, const FTransform& ShapeTransform, const FColor& Color, float LifeTime);


	/** 안쪽·바깥쪽 반지름과 부호 있는 회전 각도로 정의한 수평 환형 부채꼴을 디버그로 그립니다 */
	static void DrawDebugCombatAnnularSector(const UWorld* World, const FTransform& ShapeTransform, float InnerRadius, float OuterRadius, float StartAngleOffsetDegrees, float SweepAngleDegrees, const FColor& Color, float LifeTime);

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

private:
	/** 공용 형상 조회를 수행하고 요청한 경우에만 전달받은 중간 형상을 디버그로 그립니다 */
	static void FindTargetsInShapeInternal(const AActor* Attacker, ECollisionChannel TargetChannel, const FRSCombatShape& Shape, const FTransform& ShapeTransform, TArray<AActor*>& OutTargets, bool bShouldDrawDebug);

	/** 선택적인 방향 데이터를 포함해 공통 피격 반응 Event를 구성하고 전송합니다 */
	static void SendHitReactionInternal(const AActor* Instigator, AActor* TargetActor, const FRSHitReactionDefinition& ReactionDefinition, bool bHasKnockbackDirection, const FVector& KnockbackDirection);
};
