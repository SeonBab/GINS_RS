#include "RSPizzaPatternMath.h"

namespace
{
	bool TryGetHorizontalForward(const FTransform& Transform, FVector& OutForward)
	{
		OutForward = Transform.GetUnitAxis(EAxis::X);
		OutForward.Z = 0.0f;

		return !OutForward.ContainsNaN() && OutForward.Normalize();
	}
}

bool RSPizzaPatternMath::TryCalculateLockedPatternTransform(const FTransform& CapsuleTransform, float CapsuleHalfHeight, const FVector& ActorForward, FTransform& OutLockedTransform)
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

float RSPizzaPatternMath::CalculateSliceAngleDegrees(int32 SliceCount)
{
	if (SliceCount < 2)
	{
		return 0.0f;
	}

	return static_cast<float>(360.0 / (static_cast<double>(SliceCount) * 2.0));
}

bool RSPizzaPatternMath::TryBuildExplosionSliceTransforms(const FTransform& LockedTransform, int32 SliceCount, int32 ExplosionIndex, TArray<FTransform>& OutSliceTransforms)
{
	OutSliceTransforms.Reset();
	const float SliceAngleDegrees = CalculateSliceAngleDegrees(SliceCount);
	if (LockedTransform.ContainsNaN() || ExplosionIndex < 0 || SliceAngleDegrees <= 0.0f)
	{
		return false;
	}

	FVector HorizontalForward;
	if (!TryGetHorizontalForward(LockedTransform, HorizontalForward))
	{
		return false;
	}

	const float LockedYaw = HorizontalForward.Rotation().Yaw;
	const int32 GroupParity = ExplosionIndex % 2;
	OutSliceTransforms.Reserve(SliceCount);
	for (int32 SliceIndex = 0; SliceIndex < SliceCount; ++SliceIndex)
	{
		const int64 VirtualSliceIndex = static_cast<int64>(SliceIndex) * 2 + GroupParity;
		const float SliceYaw = LockedYaw + static_cast<float>(static_cast<double>(VirtualSliceIndex) * SliceAngleDegrees);
		OutSliceTransforms.Emplace(FRotator(0.0f, SliceYaw, 0.0f), LockedTransform.GetLocation());
	}

	return true;
}

bool RSPizzaPatternMath::TryCalculateVirtualSliceIndex(const FTransform& LockedTransform, int32 SliceCount, const FVector& TargetLocation, int64& OutVirtualSliceIndex)
{
	OutVirtualSliceIndex = INDEX_NONE;
	const float SliceAngleDegrees = CalculateSliceAngleDegrees(SliceCount);
	if (LockedTransform.ContainsNaN() || TargetLocation.ContainsNaN() || SliceAngleDegrees <= 0.0f)
	{
		return false;
	}

	FVector HorizontalForward;
	if (!TryGetHorizontalForward(LockedTransform, HorizontalForward))
	{
		return false;
	}

	FVector DirectionToTarget = TargetLocation - LockedTransform.GetLocation();
	DirectionToTarget.Z = 0.0f;
	if (!DirectionToTarget.Normalize())
	{
		// 원점에서는 각도가 정의되지 않으므로 정면 A 조각에 일관되게 배정합니다
		OutVirtualSliceIndex = 0;

		return true;
	}

	const float CrossZ = FVector::CrossProduct(HorizontalForward, DirectionToTarget).Z;
	const float Dot = FVector::DotProduct(HorizontalForward, DirectionToTarget);
	const float RelativeAngleDegrees = FMath::RadiansToDegrees(FMath::Atan2(CrossZ, Dot));
	const float NormalizedAngleDegrees = FMath::Fmod(RelativeAngleDegrees + 360.0f, 360.0f);
	const int64 VirtualSliceCount = static_cast<int64>(SliceCount) * 2;
	OutVirtualSliceIndex = FMath::FloorToInt64((NormalizedAngleDegrees + SliceAngleDegrees * 0.5f) / SliceAngleDegrees) % VirtualSliceCount;

	return true;
}

bool RSPizzaPatternMath::IsLocationInExplosionGroup(const FTransform& LockedTransform, int32 SliceCount, int32 ExplosionIndex, const FVector& TargetLocation)
{
	if (ExplosionIndex < 0)
	{
		return false;
	}

	int64 VirtualSliceIndex = INDEX_NONE;
	if (!TryCalculateVirtualSliceIndex(LockedTransform, SliceCount, TargetLocation, VirtualSliceIndex))
	{
		return false;
	}

	return VirtualSliceIndex % 2 == ExplosionIndex % 2;
}
