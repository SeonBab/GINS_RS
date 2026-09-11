// Fill out your copyright notice in the Description page of Project Settings.

#include "Combat/RSArmSwingMath.h"

namespace
{
	constexpr float MinimumSize = KINDA_SMALL_NUMBER;
	constexpr float MaximumAngularStepDegrees = 5.0f;
	constexpr float MaximumOuterArcStep = 25.0f;
	constexpr int32 MaximumSubstepCount = 16;
	constexpr float TelegraphSmallDistance = 1.0f;
	constexpr float ProgressEndpointTolerance = 0.01f;
	constexpr int32 MinimumProgressSampleCount = 2;

	bool IsFinite(float Value)
	{
		return FMath::IsFinite(Value);
	}

	void SetValidationError(FString* OutValidationError, const TCHAR* Error)
	{
		if (OutValidationError)
		{
			*OutValidationError = Error;
		}
	}

	float CalculateOuterRadius(const FRSArmSwingBoxDefinition& BoxDefinition)
	{
		const float OuterEdgeDistance = BoxDefinition.InnerOffset + BoxDefinition.BoxLength;

		return FMath::Sqrt(FMath::Square(OuterEdgeDistance) + FMath::Square(BoxDefinition.BoxHalfWidth));
	}
}

bool FRSArmSwingBoxDefinition::IsDataValid(FString* OutValidationError) const
{
	if (OutValidationError)
	{
		OutValidationError->Reset();
	}

	if (!IsFinite(InnerOffset) || !IsFinite(BoxLength) || !IsFinite(BoxHalfWidth) || !IsFinite(BoxHalfHeight) || !IsFinite(BoxCenterHeight))
	{
		SetValidationError(OutValidationError, TEXT("Arm Swing Box 값은 유한해야 합니다"));

		return false;
	}

	if (InnerOffset < 0.0f || BoxCenterHeight < 0.0f)
	{
		SetValidationError(OutValidationError, TEXT("Arm Swing Box의 Offset과 중심 높이는 0 이상이어야 합니다"));

		return false;
	}

	if (BoxLength <= MinimumSize || BoxHalfWidth <= MinimumSize || BoxHalfHeight <= MinimumSize)
	{
		SetValidationError(OutValidationError, TEXT("Arm Swing Box의 길이와 반크기는 0보다 커야 합니다"));

		return false;
	}

	return true;
}

bool FRSArmSwingPathDefinition::IsDataValid(FString* OutValidationError) const
{
	if (OutValidationError)
	{
		OutValidationError->Reset();
	}

	if (!IsFinite(StartYawOffset) || !IsFinite(SweepAngleDegrees))
	{
		SetValidationError(OutValidationError, TEXT("Arm Swing 경로 각도는 유한해야 합니다"));

		return false;
	}

	if (FMath::IsNearlyZero(SweepAngleDegrees) || FMath::Abs(SweepAngleDegrees) > 360.0f)
	{
		SetValidationError(OutValidationError, TEXT("Arm Swing Sweep 각도는 0이 아니고 360도 이하여야 합니다"));

		return false;
	}

	return true;
}

bool FRSArmSwingMath::TryCalculateLockedAttackTransform(const FTransform& CapsuleTransform, float ScaledCapsuleHalfHeight, const FVector& ActorForward, FTransform& OutLockedAttackTransform)
{
	OutLockedAttackTransform = FTransform::Identity;
	if (CapsuleTransform.ContainsNaN() || !IsFinite(ScaledCapsuleHalfHeight) || ScaledCapsuleHalfHeight < 0.0f || ActorForward.ContainsNaN())
	{
		return false;
	}

	FVector HorizontalForward = ActorForward;
	HorizontalForward.Z = 0.0f;
	if (!HorizontalForward.Normalize())
	{
		return false;
	}

	const FVector CapsuleUp = CapsuleTransform.GetUnitAxis(EAxis::Z);
	const FVector AttackOrigin = CapsuleTransform.GetLocation() - CapsuleUp * ScaledCapsuleHalfHeight;
	OutLockedAttackTransform = FTransform(HorizontalForward.Rotation(), AttackOrigin);

	return true;
}

bool FRSArmSwingMath::TryCalculateBoxSample(const FTransform& LockedAttackTransform, const FRSArmSwingBoxDefinition& BoxDefinition, const FRSArmSwingPathDefinition& PathDefinition, float Alpha, FRSArmSwingBoxSample& OutSample)
{
	OutSample = FRSArmSwingBoxSample();
	if (LockedAttackTransform.ContainsNaN() || !BoxDefinition.IsDataValid() || !PathDefinition.IsDataValid() || !IsFinite(Alpha))
	{
		return false;
	}

	const float ClampedAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
	const float LockedYaw = LockedAttackTransform.Rotator().Yaw;
	const float CurrentYawOffset = PathDefinition.StartYawOffset + PathDefinition.SweepAngleDegrees * ClampedAlpha;
	const FRotator BoxRotation(0.0f, LockedYaw + CurrentYawOffset, 0.0f);
	const FVector RadialDirection = BoxRotation.Vector();
	const float BoxCenterDistance = BoxDefinition.InnerOffset + BoxDefinition.BoxLength * 0.5f;
	const FVector BoxCenter = LockedAttackTransform.GetLocation() + RadialDirection * BoxCenterDistance + FVector::UpVector * BoxDefinition.BoxCenterHeight;
	const float TangentYawOffset = PathDefinition.SweepAngleDegrees > 0.0f ? 90.0f : -90.0f;

	OutSample.BoxTransform = FTransform(BoxRotation, BoxCenter);
	OutSample.BoxExtent = FVector(BoxDefinition.BoxLength * 0.5f, BoxDefinition.BoxHalfWidth, BoxDefinition.BoxHalfHeight);
	OutSample.RadialDirection = RadialDirection;
	OutSample.TangentDirection = RadialDirection.RotateAngleAxis(TangentYawOffset, FVector::UpVector);
	OutSample.YawOffset = CurrentYawOffset;

	return true;
}

bool FRSArmSwingMath::TryCalculateSubstepPlan(const FRSArmSwingBoxDefinition& BoxDefinition, const FRSArmSwingPathDefinition& PathDefinition, float PreviousAlpha, float CurrentAlpha, FRSArmSwingSubstepPlan& OutPlan)
{
	OutPlan = FRSArmSwingSubstepPlan();
	if (!BoxDefinition.IsDataValid() || !PathDefinition.IsDataValid() || !IsFinite(PreviousAlpha) || !IsFinite(CurrentAlpha))
	{
		return false;
	}

	const float ClampedPreviousAlpha = FMath::Clamp(PreviousAlpha, 0.0f, 1.0f);
	const float ClampedCurrentAlpha = FMath::Clamp(CurrentAlpha, 0.0f, 1.0f);
	const float DeltaAngleDegrees = FMath::Abs(PathDefinition.SweepAngleDegrees * (ClampedCurrentAlpha - ClampedPreviousAlpha));
	const float OuterArcDistance = CalculateOuterRadius(BoxDefinition) * FMath::DegreesToRadians(DeltaAngleDegrees);
	const int32 RequiredByAngle = FMath::CeilToInt(DeltaAngleDegrees / MaximumAngularStepDegrees);
	const int32 RequiredByDistance = FMath::CeilToInt(OuterArcDistance / MaximumOuterArcStep);

	OutPlan.RequiredStepCount = FMath::Max(1, FMath::Max(RequiredByAngle, RequiredByDistance));
	OutPlan.StepCount = FMath::Min(OutPlan.RequiredStepCount, MaximumSubstepCount);
	OutPlan.bReachedLimit = OutPlan.RequiredStepCount > MaximumSubstepCount;

	return true;
}

bool FRSArmSwingMath::TryCalculateTelegraphBounds(const FRSArmSwingBoxDefinition& BoxDefinition, const FRSArmSwingPathDefinition& PathDefinition, FRSArmSwingTelegraphBounds& OutBounds)
{
	OutBounds = FRSArmSwingTelegraphBounds();
	if (!BoxDefinition.IsDataValid() || !PathDefinition.IsDataValid())
	{
		return false;
	}

	const float AngularPadding = FMath::RadiansToDegrees(FMath::Atan2(BoxDefinition.BoxHalfWidth, FMath::Max(BoxDefinition.InnerOffset, TelegraphSmallDistance)));
	const float SweepSign = FMath::Sign(PathDefinition.SweepAngleDegrees);

	OutBounds.InnerRadius = BoxDefinition.InnerOffset;
	OutBounds.OuterRadius = CalculateOuterRadius(BoxDefinition);
	OutBounds.StartYawOffset = PathDefinition.StartYawOffset - SweepSign * AngularPadding;
	const float PaddedSweepAngleDegrees = FMath::Abs(PathDefinition.SweepAngleDegrees) + AngularPadding * 2.0f;
	OutBounds.SweepAngleDegrees = SweepSign * FMath::Min(PaddedSweepAngleDegrees, 360.0f);

	return true;
}

bool FRSArmSwingMath::IsProgressSampleSequenceValid(const TArray<float>& ProgressSamples, FString* OutValidationError)
{
	if (OutValidationError)
	{
		OutValidationError->Reset();
	}

	if (ProgressSamples.Num() < MinimumProgressSampleCount)
	{
		SetValidationError(OutValidationError, TEXT("Arm Swing 회전 진행률은 최소 두 개 Sample이 필요합니다"));

		return false;
	}

	for (const float ProgressSample : ProgressSamples)
	{
		if (!IsFinite(ProgressSample))
		{
			SetValidationError(OutValidationError, TEXT("Arm Swing 회전 진행률 Sample은 유한해야 합니다"));

			return false;
		}
	}

	if (!FMath::IsNearlyZero(ProgressSamples[0], ProgressEndpointTolerance))
	{
		SetValidationError(OutValidationError, TEXT("Arm Swing 회전 진행률은 Attack Window 시작에서 0이어야 합니다"));

		return false;
	}

	if (!FMath::IsNearlyEqual(ProgressSamples.Last(), 1.0f, ProgressEndpointTolerance))
	{
		SetValidationError(OutValidationError, TEXT("Arm Swing 회전 진행률은 Attack Window 끝에서 1이어야 합니다"));

		return false;
	}

	for (int32 SampleIndex = 1; SampleIndex < ProgressSamples.Num(); ++SampleIndex)
	{
		// 진행률이 되돌아가면 Box가 역회전하고 접선 방향 부호가 뒤집혀 넉백 방향이 반대가 됩니다
		if (ProgressSamples[SampleIndex] < ProgressSamples[SampleIndex - 1] - ProgressEndpointTolerance)
		{
			SetValidationError(OutValidationError, TEXT("Arm Swing 회전 진행률은 되돌아가지 않고 단조 증가해야 합니다"));

			return false;
		}
	}

	return true;
}
