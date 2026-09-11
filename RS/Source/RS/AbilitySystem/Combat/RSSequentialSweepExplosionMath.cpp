#include "RSSequentialSweepExplosionMath.h"

namespace
{
	constexpr float AngleBoundaryToleranceDegrees = 0.01f;

	bool TryGetHorizontalSweepForward(const FTransform& Transform, FVector& OutForward)
	{
		OutForward = Transform.GetUnitAxis(EAxis::X);
		OutForward.Z = 0.0f;

		return !OutForward.ContainsNaN() && OutForward.Normalize();
	}
}

bool RSSequentialSweepExplosionMath::TryCalculateLockedAttackTransform(const FTransform& CapsuleTransform, float CapsuleHalfHeight, const FVector& ActorForward, FTransform& OutLockedTransform)
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

	FVector CapsuleUp = CapsuleTransform.GetUnitAxis(EAxis::Z);
	if (CapsuleUp.ContainsNaN() || !CapsuleUp.Normalize())
	{
		return false;
	}

	const FVector AttackOrigin = CapsuleTransform.GetLocation() - CapsuleUp * CapsuleHalfHeight;
	OutLockedTransform = FTransform(HorizontalForward.Rotation(), AttackOrigin);

	return !OutLockedTransform.ContainsNaN();
}

float RSSequentialSweepExplosionMath::CalculateSectorAngleDegrees(float TotalSweepAngleDegrees, int32 SectorCount)
{
	if (!FMath::IsFinite(TotalSweepAngleDegrees) || TotalSweepAngleDegrees <= 0.0f || TotalSweepAngleDegrees > 360.0f || SectorCount < 1)
	{
		return 0.0f;
	}

	return TotalSweepAngleDegrees / static_cast<float>(SectorCount);
}

float RSSequentialSweepExplosionMath::CalculateTotalDuration(float FirstExplosionDelay, float SectorExplosionInterval, int32 SectorCount)
{
	if (!FMath::IsFinite(FirstExplosionDelay) || FirstExplosionDelay <= 0.0f
		|| !FMath::IsFinite(SectorExplosionInterval) || SectorExplosionInterval <= 0.0f
		|| SectorCount < 1)
	{
		return 0.0f;
	}

	return FirstExplosionDelay + static_cast<float>(SectorCount - 1) * SectorExplosionInterval;
}

int32 RSSequentialSweepExplosionMath::CalculateRequiredWarningCount(float ElapsedTime, float FirstExplosionDelay, int32 SectorCount)
{
	if (!FMath::IsFinite(ElapsedTime) || ElapsedTime < 0.0f || !FMath::IsFinite(FirstExplosionDelay) || FirstExplosionDelay <= 0.0f || SectorCount < 1)
	{
		return 0;
	}

	const float WarningInterval = FirstExplosionDelay / static_cast<float>(SectorCount);
	const int32 RequiredWarningCount = FMath::FloorToInt(ElapsedTime / WarningInterval + UE_KINDA_SMALL_NUMBER) + 1;

	return FMath::Clamp(RequiredWarningCount, 1, SectorCount);
}

int32 RSSequentialSweepExplosionMath::CalculateRequiredExplosionCount(float ElapsedTime, float FirstExplosionDelay, float SectorExplosionInterval, int32 SectorCount)
{
	if (!FMath::IsFinite(ElapsedTime) || ElapsedTime < 0.0f
		|| !FMath::IsFinite(FirstExplosionDelay) || FirstExplosionDelay <= 0.0f
		|| !FMath::IsFinite(SectorExplosionInterval) || SectorExplosionInterval <= 0.0f
		|| SectorCount < 1
		|| ElapsedTime + UE_KINDA_SMALL_NUMBER < FirstExplosionDelay)
	{
		return 0;
	}

	const float TimeAfterFirstExplosion = FMath::Max(ElapsedTime - FirstExplosionDelay, 0.0f);
	const int32 RequiredExplosionCount = FMath::FloorToInt(TimeAfterFirstExplosion / SectorExplosionInterval + UE_KINDA_SMALL_NUMBER) + 1;

	return FMath::Clamp(RequiredExplosionCount, 1, SectorCount);
}

bool RSSequentialSweepExplosionMath::TryCalculateSectorAngles(float TotalSweepAngleDegrees, int32 SectorCount, float StartAngleOffsetDegrees, int32 SectorIndex, float& OutStartAngleOffsetDegrees, float& OutSweepAngleDegrees)
{
	OutStartAngleOffsetDegrees = 0.0f;
	OutSweepAngleDegrees = 0.0f;
	const float SectorAngleDegrees = CalculateSectorAngleDegrees(TotalSweepAngleDegrees, SectorCount);
	if (!FMath::IsFinite(StartAngleOffsetDegrees) || SectorIndex < 0 || SectorIndex >= SectorCount || SectorAngleDegrees <= 0.0f)
	{
		return false;
	}

	OutStartAngleOffsetDegrees = StartAngleOffsetDegrees + static_cast<float>(SectorIndex) * SectorAngleDegrees;
	OutSweepAngleDegrees = SectorAngleDegrees;

	return FMath::IsFinite(OutStartAngleOffsetDegrees);
}

bool RSSequentialSweepExplosionMath::IsLocationInSector(const FTransform& LockedTransform, float OuterRadius, float TotalSweepAngleDegrees, int32 SectorCount, float StartAngleOffsetDegrees, int32 SectorIndex, const FVector& TargetLocation)
{
	float SectorStartAngleDegrees = 0.0f;
	float SectorAngleDegrees = 0.0f;
	if (LockedTransform.ContainsNaN() || TargetLocation.ContainsNaN() || !FMath::IsFinite(OuterRadius) || OuterRadius <= 0.0f
		|| !TryCalculateSectorAngles(TotalSweepAngleDegrees, SectorCount, StartAngleOffsetDegrees, SectorIndex, SectorStartAngleDegrees, SectorAngleDegrees))
	{
		return false;
	}

	FVector DirectionToTarget = TargetLocation - LockedTransform.GetLocation();
	DirectionToTarget.Z = 0.0f;
	const float DistanceSquared = DirectionToTarget.SizeSquared();
	if (DistanceSquared > FMath::Square(OuterRadius) + UE_KINDA_SMALL_NUMBER)
	{
		return false;
	}

	// 모든 조각이 같은 원점을 공유하므로 중심은 각 부채꼴 판정에 포함합니다
	if (!DirectionToTarget.Normalize())
	{
		return true;
	}

	FVector LockedForward;
	if (!TryGetHorizontalSweepForward(LockedTransform, LockedForward))
	{
		return false;
	}

	const FVector SectorStartDirection = LockedForward.RotateAngleAxis(SectorStartAngleDegrees, FVector::UpVector);
	const float CrossZ = FVector::CrossProduct(SectorStartDirection, DirectionToTarget).Z;
	const float Dot = FVector::DotProduct(SectorStartDirection, DirectionToTarget);
	const float SignedAngleDegrees = FMath::RadiansToDegrees(FMath::Atan2(CrossZ, Dot));
	const float ClockwiseAngleDegrees = FMath::Fmod(SignedAngleDegrees + 360.0f, 360.0f);

	// 시작과 끝 경계를 모두 포함해 인접한 두 조각이 같은 경계 대상을 각각 처리할 수 있게 합니다
	return ClockwiseAngleDegrees <= SectorAngleDegrees + AngleBoundaryToleranceDegrees
		|| FMath::IsNearlyEqual(ClockwiseAngleDegrees, 360.0f, AngleBoundaryToleranceDegrees);
}
