// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RSArmSwingMath.generated.h"

/** Arm Swing의 회전형 Box가 사용할 공통 공간 데이터입니다 */
USTRUCT(BlueprintType)
struct FRSArmSwingBoxDefinition
{
	GENERATED_BODY()

	/** 공격 Pivot에서 Box 안쪽 면까지의 수평 거리입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Combat|Arm Swing", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm"))
	float InnerOffset = 0.0f;

	/** 안쪽 면부터 바깥 방향으로 뻗는 Box의 전체 길이입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Combat|Arm Swing", meta = (ClampMin = "0.0", UIMin = "1.0", ForceUnits = "cm"))
	float BoxLength = 500.0f;

	/** Box의 수평 반폭입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Combat|Arm Swing", meta = (ClampMin = "0.0", UIMin = "1.0", ForceUnits = "cm"))
	float BoxHalfWidth = 100.0f;

	/** Box의 수직 반높이입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Combat|Arm Swing", meta = (ClampMin = "0.0", UIMin = "1.0", ForceUnits = "cm"))
	float BoxHalfHeight = 100.0f;

	/** 발밑 공격 Pivot에서 Box 중심까지의 높이입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Combat|Arm Swing", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm"))
	float BoxCenterHeight = 200.0f;

	/** 회전형 Box 계산에 사용할 수 있는 값인지 검사합니다 */
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

/** 한 진행률에서 계산한 Arm Swing Box의 월드 공간 Sample입니다 */
struct FRSArmSwingBoxSample
{
	FTransform BoxTransform = FTransform::Identity;
	FVector BoxExtent = FVector::ZeroVector;
	FVector RadialDirection = FVector::ZeroVector;
	FVector TangentDirection = FVector::ZeroVector;
	float YawOffset = 0.0f;
};

/** 이전 진행률과 현재 진행률 사이를 검사할 Substep 계획입니다 */
struct FRSArmSwingSubstepPlan
{
	int32 StepCount = 1;
	int32 RequiredStepCount = 1;
	bool bReachedLimit = false;
};

/** 회전형 Box 전체를 감싸는 환형 부채꼴 Telegraph 데이터입니다 */
struct FRSArmSwingTelegraphBounds
{
	float InnerRadius = 0.0f;
	float OuterRadius = 0.0f;
	float StartYawOffset = 0.0f;
	float SweepAngleDegrees = 0.0f;
};

/** Arm Swing 전용 공간 계산을 제공합니다 */
struct RS_API FRSArmSwingMath
{
	/** Capsule 발밑과 Actor 전방에서 공격 중 고정할 월드 Transform을 계산합니다 */
	static bool TryCalculateLockedAttackTransform(const FTransform& CapsuleTransform, float ScaledCapsuleHalfHeight, const FVector& ActorForward, FTransform& OutLockedAttackTransform);

	/** 고정된 공격 Transform과 진행률에서 현재 Box Sample을 계산합니다 */
	static bool TryCalculateBoxSample(const FTransform& LockedAttackTransform, const FRSArmSwingBoxDefinition& BoxDefinition, const FRSArmSwingPathDefinition& PathDefinition, float Alpha, FRSArmSwingBoxSample& OutSample);

	/** 두 진행률 사이에서 필요한 적응형 Substep 개수를 계산합니다 */
	static bool TryCalculateSubstepPlan(const FRSArmSwingBoxDefinition& BoxDefinition, const FRSArmSwingPathDefinition& PathDefinition, float PreviousAlpha, float CurrentAlpha, FRSArmSwingSubstepPlan& OutPlan);

	/** 회전형 Box의 전체 수평 경로를 빠짐없이 감싸는 Telegraph 경계를 계산합니다 */
	static bool TryCalculateTelegraphBounds(const FRSArmSwingBoxDefinition& BoxDefinition, const FRSArmSwingPathDefinition& PathDefinition, FRSArmSwingTelegraphBounds& OutBounds);

};
