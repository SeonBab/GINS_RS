#include "RSCircularSliceMath.h"

namespace
{
	bool TryGetCircularSliceHorizontalForward(const FTransform& Transform, FVector& OutForward)
	{
		OutForward = Transform.GetUnitAxis(EAxis::X);
		OutForward.Z = 0.0f;

		return !OutForward.ContainsNaN() && OutForward.Normalize();
	}
}

bool RSCircularSliceMath::TryCalculateLockedTransformFromCapsule(const FTransform& CapsuleTransform, float CapsuleHalfHeight, const FVector& ActorForward, FTransform& OutLockedTransform)
{
	OutLockedTransform = FTransform::Identity;
	if (CapsuleTransform.ContainsNaN() || !FMath::IsFinite(CapsuleHalfHeight) || CapsuleHalfHeight <= 0.0f || ActorForward.ContainsNaN())
	{
		return false;
	}

	FVector HorizontalForward = ActorForward;
	HorizontalForward.Z = 0.0f;
	if (!HorizontalForward.Normalize())
	{
		return false;
	}

	const FVector PatternLocation = CapsuleTransform.GetLocation() - FVector::UpVector * CapsuleHalfHeight;
	const FQuat PatternRotation = HorizontalForward.Rotation().Quaternion();
	OutLockedTransform = FTransform(PatternRotation, PatternLocation);

	return !OutLockedTransform.ContainsNaN();
}

float RSCircularSliceMath::CalculateSliceAngleDegrees(int32 TotalSliceCount)
{
	if (TotalSliceCount < 2)
	{
		return 0.0f;
	}

	return static_cast<float>(360.0 / static_cast<double>(TotalSliceCount));
}

bool RSCircularSliceMath::TryBuildSliceTransform(const FTransform& LockedTransform, int32 TotalSliceCount, int32 SliceIndex, FTransform& OutSliceTransform)
{
	OutSliceTransform = FTransform::Identity;
	const float SliceAngleDegrees = CalculateSliceAngleDegrees(TotalSliceCount);
	if (LockedTransform.ContainsNaN() || SliceIndex < 0 || SliceIndex >= TotalSliceCount || SliceAngleDegrees <= 0.0f)
	{
		return false;
	}

	FVector HorizontalForward;
	if (!TryGetCircularSliceHorizontalForward(LockedTransform, HorizontalForward))
	{
		return false;
	}

	const float LockedYaw = HorizontalForward.Rotation().Yaw;
	const float SliceYaw = LockedYaw + static_cast<float>(static_cast<double>(SliceIndex) * SliceAngleDegrees);
	OutSliceTransform = FTransform(FRotator(0.0f, SliceYaw, 0.0f), LockedTransform.GetLocation());

	return true;
}

bool RSCircularSliceMath::TryCalculateSliceIndex(const FTransform& LockedTransform, int32 TotalSliceCount, const FVector& TargetLocation, int32& OutSliceIndex)
{
	OutSliceIndex = INDEX_NONE;
	const float SliceAngleDegrees = CalculateSliceAngleDegrees(TotalSliceCount);
	if (LockedTransform.ContainsNaN() || TargetLocation.ContainsNaN() || SliceAngleDegrees <= 0.0f)
	{
		return false;
	}

	FVector HorizontalForward;
	if (!TryGetCircularSliceHorizontalForward(LockedTransform, HorizontalForward))
	{
		return false;
	}

	FVector DirectionToTarget = TargetLocation - LockedTransform.GetLocation();
	DirectionToTarget.Z = 0.0f;
	if (!DirectionToTarget.Normalize())
	{
		// 원점에서는 각도가 정의되지 않으므로 정면의 0번 조각에 일관되게 배정합니다
		OutSliceIndex = 0;

		return true;
	}

	const float CrossZ = FVector::CrossProduct(HorizontalForward, DirectionToTarget).Z;
	const float Dot = FVector::DotProduct(HorizontalForward, DirectionToTarget);
	const float RelativeAngleDegrees = FMath::RadiansToDegrees(FMath::Atan2(CrossZ, Dot));
	const float NormalizedAngleDegrees = FMath::Fmod(RelativeAngleDegrees + 360.0f, 360.0f);
	OutSliceIndex = FMath::FloorToInt((NormalizedAngleDegrees + SliceAngleDegrees * 0.5f) / SliceAngleDegrees) % TotalSliceCount;

	return true;
}

bool RSCircularSliceMath::AreSlicesOpposite(int32 TotalSliceCount, int32 FirstSliceIndex, int32 SecondSliceIndex)
{
	if (TotalSliceCount < 2 || TotalSliceCount % 2 != 0
		|| FirstSliceIndex < 0 || FirstSliceIndex >= TotalSliceCount
		|| SecondSliceIndex < 0 || SecondSliceIndex >= TotalSliceCount)
	{
		return false;
	}

	return FMath::Abs(FirstSliceIndex - SecondSliceIndex) == TotalSliceCount / 2;
}
