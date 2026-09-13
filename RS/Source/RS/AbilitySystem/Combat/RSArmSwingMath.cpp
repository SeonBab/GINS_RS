// Fill out your copyright notice in the Description page of Project Settings.

#include "Combat/RSArmSwingMath.h"

namespace
{
	constexpr float MinimumSize = KINDA_SMALL_NUMBER;
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

	float CalculateOuterRadius(const FRSArmSwingSectorDefinition& SectorDefinition)
	{
		const float OuterEdgeDistance = SectorDefinition.InnerOffset + SectorDefinition.RadialLength;

		return FMath::Sqrt(FMath::Square(OuterEdgeDistance) + FMath::Square(SectorDefinition.PathHalfWidth));
	}
}

bool FRSArmSwingSectorDefinition::IsDataValid(FString* OutValidationError) const
{
	if (OutValidationError)
	{
		OutValidationError->Reset();
	}

	if (!IsFinite(InnerOffset) || !IsFinite(RadialLength) || !IsFinite(PathHalfWidth))
	{
		SetValidationError(OutValidationError, TEXT("Arm Swing 수평 범위 값은 유한해야 합니다"));

		return false;
	}

	if (InnerOffset < 0.0f)
	{
		SetValidationError(OutValidationError, TEXT("Arm Swing의 안쪽 Offset은 0 이상이어야 합니다"));

		return false;
	}

	if (RadialLength <= MinimumSize || PathHalfWidth <= MinimumSize)
	{
		SetValidationError(OutValidationError, TEXT("Arm Swing의 수평 길이와 반폭은 0보다 커야 합니다"));

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

bool FRSArmSwingMath::TryCalculateSectorBounds(const FRSArmSwingSectorDefinition& SectorDefinition, const FRSArmSwingPathDefinition& PathDefinition, float PreviousProgress, float CurrentProgress, FRSArmSwingSectorBounds& OutBounds)
{
	OutBounds = FRSArmSwingSectorBounds();
	if (!SectorDefinition.IsDataValid() || !PathDefinition.IsDataValid() || !IsFinite(PreviousProgress) || !IsFinite(CurrentProgress))
	{
		return false;
	}

	const float ClampedPreviousProgress = FMath::Clamp(PreviousProgress, 0.0f, 1.0f);
	const float ClampedCurrentProgress = FMath::Clamp(CurrentProgress, 0.0f, 1.0f);
	if (ClampedCurrentProgress < ClampedPreviousProgress - KINDA_SMALL_NUMBER)
	{
		return false;
	}

	// Pivot과 맞닿은 경로도 거의 전 방향으로 과대 표시되지 않도록 중심 회전 반지름에서 수평 반폭을 각도로 변환합니다
	const float PathCenterRadius = SectorDefinition.InnerOffset + SectorDefinition.RadialLength * 0.5f;
	const float AngularPadding = FMath::RadiansToDegrees(FMath::Atan2(SectorDefinition.PathHalfWidth, PathCenterRadius));
	const float SweepSign = FMath::Sign(PathDefinition.SweepAngleDegrees);
	const float IntervalSweepAngleDegrees = PathDefinition.SweepAngleDegrees * (ClampedCurrentProgress - ClampedPreviousProgress);

	OutBounds.InnerRadius = SectorDefinition.InnerOffset;
	OutBounds.OuterRadius = CalculateOuterRadius(SectorDefinition);
	OutBounds.StartYawOffset = PathDefinition.StartYawOffset + PathDefinition.SweepAngleDegrees * ClampedPreviousProgress - SweepSign * AngularPadding;
	const float PaddedSweepAngleDegrees = FMath::Abs(IntervalSweepAngleDegrees) + AngularPadding * 2.0f;
	OutBounds.SweepAngleDegrees = SweepSign * FMath::Min(PaddedSweepAngleDegrees, 360.0f);

	return true;
}

bool FRSArmSwingMath::TryCalculateTelegraphBounds(const FRSArmSwingSectorDefinition& SectorDefinition, const FRSArmSwingPathDefinition& PathDefinition, FRSArmSwingSectorBounds& OutBounds)
{
	return TryCalculateSectorBounds(SectorDefinition, PathDefinition, 0.0f, 1.0f, OutBounds);
}

bool FRSArmSwingMath::IsLocationInsideSector(const FTransform& LockedAttackTransform, const FRSArmSwingSectorBounds& SectorBounds, const FVector& TargetLocation)
{
	if (LockedAttackTransform.ContainsNaN() || TargetLocation.ContainsNaN()
		|| !IsFinite(SectorBounds.InnerRadius) || !IsFinite(SectorBounds.OuterRadius)
		|| !IsFinite(SectorBounds.StartYawOffset) || !IsFinite(SectorBounds.SweepAngleDegrees)
		|| SectorBounds.InnerRadius < 0.0f || SectorBounds.OuterRadius <= SectorBounds.InnerRadius
		|| FMath::IsNearlyZero(SectorBounds.SweepAngleDegrees) || FMath::Abs(SectorBounds.SweepAngleDegrees) > 360.0f)
	{
		return false;
	}

	const FVector LocalOffset = LockedAttackTransform.InverseTransformPositionNoScale(TargetLocation);
	const float RadiusSquared = FMath::Square(LocalOffset.X) + FMath::Square(LocalOffset.Y);
	if (RadiusSquared < FMath::Square(SectorBounds.InnerRadius) - KINDA_SMALL_NUMBER
		|| RadiusSquared > FMath::Square(SectorBounds.OuterRadius) + KINDA_SMALL_NUMBER)
	{
		return false;
	}

	if (RadiusSquared <= KINDA_SMALL_NUMBER || FMath::IsNearlyEqual(FMath::Abs(SectorBounds.SweepAngleDegrees), 360.0f))
	{
		return true;
	}

	const float TargetYawOffset = FMath::RadiansToDegrees(FMath::Atan2(LocalOffset.Y, LocalOffset.X));
	const float DirectedAngle = SectorBounds.SweepAngleDegrees > 0.0f
		? FRotator::ClampAxis(TargetYawOffset - SectorBounds.StartYawOffset)
		: FRotator::ClampAxis(SectorBounds.StartYawOffset - TargetYawOffset);

	return DirectedAngle <= FMath::Abs(SectorBounds.SweepAngleDegrees) + KINDA_SMALL_NUMBER;
}

bool FRSArmSwingMath::TryCalculateTargetTangentDirection(const FTransform& LockedAttackTransform, const FRSArmSwingPathDefinition& PathDefinition, float SweepProgress, const FVector& TargetLocation, FVector& OutTangentDirection)
{
	OutTangentDirection = FVector::ZeroVector;
	if (LockedAttackTransform.ContainsNaN() || TargetLocation.ContainsNaN() || !PathDefinition.IsDataValid() || !IsFinite(SweepProgress))
	{
		return false;
	}

	FVector RadialDirection = TargetLocation - LockedAttackTransform.GetLocation();
	RadialDirection.Z = 0.0f;
	if (!RadialDirection.Normalize())
	{
		const float CurrentYawOffset = PathDefinition.StartYawOffset + PathDefinition.SweepAngleDegrees * FMath::Clamp(SweepProgress, 0.0f, 1.0f);
		RadialDirection = LockedAttackTransform.GetUnitAxis(EAxis::X).RotateAngleAxis(CurrentYawOffset, FVector::UpVector);
		RadialDirection.Z = 0.0f;
		if (!RadialDirection.Normalize())
		{
			return false;
		}
	}

	const float TangentYawOffset = PathDefinition.SweepAngleDegrees > 0.0f ? 90.0f : -90.0f;
	OutTangentDirection = RadialDirection.RotateAngleAxis(TangentYawOffset, FVector::UpVector);

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
