// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RSArmSwingMath.generated.h"

/** Arm Swing의 수평 환형 부채꼴을 유도할 공통 공간 데이터입니다 */
USTRUCT(BlueprintType)
struct FRSArmSwingSectorDefinition
{
	GENERATED_BODY()

	/** 공격 Pivot에서 Box 안쪽 면까지의 수평 거리입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Combat|Arm Swing", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm"))
	float InnerOffset = 0.0f;

	/** 안쪽 면부터 바깥 방향으로 뻗는 Box의 전체 길이입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Combat|Arm Swing", meta = (ClampMin = "0.0", UIMin = "1.0", ForceUnits = "cm"))
	float RadialLength = 500.0f;

	/** Box의 수평 반폭입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Combat|Arm Swing", meta = (ClampMin = "0.0", UIMin = "1.0", ForceUnits = "cm"))
	float PathHalfWidth = 100.0f;

	/** 수평 환형 부채꼴 계산에 사용할 수 있는 값인지 검사합니다 */
	bool IsDataValid(FString* OutValidationError = nullptr) const;
};

/** Arm Swing의 한 Side가 사용할 시작 각도와 회전 방향입니다 */
USTRUCT(BlueprintType)
struct FRSArmSwingPathDefinition
{
	GENERATED_BODY()

	/** 고정된 공격 기준 Yaw에서 Box가 시작되는 상대 각도입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Combat|Arm Swing", meta = (ForceUnits = "deg"))
	float StartYawOffset = 0.0f;

	/** 공격 Window 전체에서 Box가 회전할 각도이며 부호가 진행 방향을 결정합니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Combat|Arm Swing", meta = (ClampMin = "-360.0", ClampMax = "360.0", UIMin = "-180.0", UIMax = "180.0", ForceUnits = "deg"))
	float SweepAngleDegrees = 160.0f;

	/** 회전 경로와 접선 방향 계산에 사용할 수 있는 값인지 검사합니다 */
	bool IsDataValid(FString* OutValidationError = nullptr) const;
};

/** 중심 회전 경로와 수평 반폭에서 유도한 환형 부채꼴 경계입니다 */
struct FRSArmSwingSectorBounds
{
	float InnerRadius = 0.0f;
	float OuterRadius = 0.0f;
	float StartYawOffset = 0.0f;
	float SweepAngleDegrees = 0.0f;
};

/** Arm Swing 전용 공간 계산을 제공합니다 */
struct RS_API FRSArmSwingMath
{
	/** 후보 원의 열린 바깥 경계가 최종 Sector의 닫힌 경계를 선제 탈락시키지 않게 하는 내부 여유입니다 */
	static constexpr float CandidateQueryRadiusMargin = 1.0f;

	/** Capsule 발밑과 Actor 전방에서 공격 중 고정할 월드 Transform을 계산합니다 */
	static bool TryCalculateLockedAttackTransform(const FTransform& CapsuleTransform, float ScaledCapsuleHalfHeight, const FVector& ActorForward, FTransform& OutLockedAttackTransform);

	/** 지정한 진행률 구간에 수평 반폭을 반영한 환형 부채꼴 경계를 계산합니다 */
	static bool TryCalculateSectorBounds(const FRSArmSwingSectorDefinition& SectorDefinition, const FRSArmSwingPathDefinition& PathDefinition, float PreviousProgress, float CurrentProgress, FRSArmSwingSectorBounds& OutBounds);

	/** 전체 회전 경로와 실제 판정이 공유할 Telegraph 경계를 계산합니다 */
	static bool TryCalculateTelegraphBounds(const FRSArmSwingSectorDefinition& SectorDefinition, const FRSArmSwingPathDefinition& PathDefinition, FRSArmSwingSectorBounds& OutBounds);

	/** 대상 중심점이 지정한 환형 부채꼴 경계 안에 있는지 검사합니다 */
	static bool IsLocationInsideSector(const FTransform& LockedAttackTransform, const FRSArmSwingSectorBounds& SectorBounds, const FVector& TargetLocation);

	/** 대상 위치의 반지름 방향에서 Sweep 진행 방향의 수평 접선을 계산합니다 */
	static bool TryCalculateTargetTangentDirection(const FTransform& LockedAttackTransform, const FRSArmSwingPathDefinition& PathDefinition, float SweepProgress, const FVector& TargetLocation, FVector& OutTangentDirection);

	/** Attack Window 구간에서 Sample한 회전 진행률이 0에서 1까지 단조 증가하는지 검사합니다 */
	static bool IsProgressSampleSequenceValid(const TArray<float>& ProgressSamples, FString* OutValidationError = nullptr);

};
