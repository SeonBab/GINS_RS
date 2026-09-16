// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RSCombatFunctionLibrary.h"
#include "RSArmSwingMath.generated.h"

/** Arm Swing의 순간 수평 환형 부채꼴 크기입니다 */
USTRUCT(BlueprintType)
struct FRSArmSwingSectorDefinition
{
	GENERATED_BODY()

	/** 공격 Pivot에서 공격 영역의 안쪽 경계까지의 수평 반경입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Combat|Arm Swing", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm"))
	float InnerRadius = 0.0f;

	/** 공격 Pivot에서 공격 영역의 바깥 경계까지의 수평 반경입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Combat|Arm Swing", meta = (ClampMin = "0.0", UIMin = "1.0", ForceUnits = "cm"))
	float OuterRadius = 500.0f;

	/** 한 순간에 공격 판정이 차지하는 전체 각도 폭이며 이동 방향 쪽으로 펼쳐집니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Combat|Arm Swing", meta = (ClampMin = "0.01", ClampMax = "360.0", UIMin = "1.0", UIMax = "180.0", ForceUnits = "deg"))
	float AngularWidthDegrees = 40.0f;

	/** 수평 환형 부채꼴 계산에 사용할 수 있는 값인지 검사합니다 */
	bool IsDataValid(FString* OutValidationError = nullptr) const;
};

/** Arm Swing의 한 Side가 사용할 시작 각도와 회전 방향입니다 */
USTRUCT(BlueprintType)
struct FRSArmSwingPathDefinition
{
	GENERATED_BODY()

	/** 고정된 공격 기준 Yaw에서 실제 Sector의 첫 경계가 시작되는 상대 각도입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Combat|Arm Swing", meta = (ForceUnits = "deg"))
	float StartYawOffset = 0.0f;

	/** 공격 Window 동안 Sector의 시작 경계가 이동할 각도이며 부호가 진행 방향을 결정합니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Combat|Arm Swing", meta = (ClampMin = "-360.0", ClampMax = "360.0", UIMin = "-180.0", UIMax = "180.0", ForceUnits = "deg"))
	float TravelAngleDegrees = 160.0f;

	/** 회전 경로와 접선 방향 계산에 사용할 수 있는 값인지 검사합니다 */
	bool IsDataValid(FString* OutValidationError = nullptr) const;
};

/** Arm Swing 전용 공간 계산을 제공합니다 */
struct RS_API FRSArmSwingMath
{
	/**
	 * 후보 수집과 최종 판정이 바깥 경계를 각자 계산하므로 부동소수점 오차만큼 어긋날 수 있어 두는 내부 여유입니다
	 * 후보를 넓히기만 하고 실제 적중 여부는 최종 Sector 필터가 그대로 소유합니다
	 */
	static constexpr float CandidateQueryRadiusMargin = 1.0f;

	/** Capsule 발밑과 Actor 전방에서 공격 중 고정할 월드 Transform을 계산합니다 */
	static bool TryCalculateLockedAttackTransform(const FTransform& CapsuleTransform, float ScaledCapsuleHalfHeight, const FVector& ActorForward, FTransform& OutLockedAttackTransform);

	/**
	 * 지정한 진행률 구간의 이동각과 순간 각도 폭을 합친 환형 부채꼴 경계를 계산하고 안쪽 반지름을 하한까지 밀어 올립니다
	 * 예고와 판정이 모두 이 계산을 지나므로 하한을 인자로 받아야 두 경로가 같은 안쪽 경계를 봅니다
	 */
	static bool TryCalculateSectorBounds(const FRSArmSwingSectorDefinition& SectorDefinition, const FRSArmSwingPathDefinition& PathDefinition, float PreviousProgress, float CurrentProgress, float MinimumInnerRadius, FRSAnnularSectorBounds& OutBounds);

	/** 전체 회전 경로와 실제 판정이 공유할 Telegraph 경계를 계산합니다 */
	static bool TryCalculateTelegraphBounds(const FRSArmSwingSectorDefinition& SectorDefinition, const FRSArmSwingPathDefinition& PathDefinition, float MinimumInnerRadius, FRSAnnularSectorBounds& OutBounds);

	/** 대상 위치의 반지름 방향에서 Sector 이동 방향의 수평 접선을 계산합니다 */
	static bool TryCalculateTargetTangentDirection(const FTransform& LockedAttackTransform, const FRSArmSwingPathDefinition& PathDefinition, float SweepProgress, const FVector& TargetLocation, FVector& OutTangentDirection);

	/** Attack Window 구간에서 Sample한 회전 진행률이 0에서 1까지 단조 증가하는지 검사합니다 */
	static bool IsProgressSampleSequenceValid(const TArray<float>& ProgressSamples, FString* OutValidationError = nullptr);

};
