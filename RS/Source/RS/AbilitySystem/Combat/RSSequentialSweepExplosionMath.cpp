#include "RSSequentialSweepExplosionMath.h"

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

bool RSSequentialSweepExplosionMath::IsLocationInSector(const FTransform& LockedTransform, float InnerRadius, float OuterRadius, float TotalSweepAngleDegrees, int32 SectorCount, float StartAngleOffsetDegrees, int32 SectorIndex, const FVector& TargetLocation)
{
	// 예고, 연출과 같은 형상을 만들어 공용 커널에 넘기므로 조각의 경계 규약이 다른 형상과 갈라질 수 없습니다
	FRSCombatShape SectorShape;
	FTransform SectorTransform;
	if (!TryBuildSectorFillShape(LockedTransform, OuterRadius, TotalSweepAngleDegrees, SectorCount, StartAngleOffsetDegrees, SectorIndex, SectorShape, SectorTransform))
	{
		return false;
	}

	// 하한은 호출자가 이번 활성화에 확정한 값이므로 여기서 다시 올리지 않고 그대로 담습니다
	SectorShape.InnerRadius = InnerRadius;

	return URSCombatFunctionLibrary::IsLocationInsideAnnularSector(SectorTransform, SectorShape.GetAnnularSectorBounds(), TargetLocation);
}

bool RSSequentialSweepExplosionMath::TryBuildSectorFillShape(const FTransform& LockedTransform, float OuterRadius, float TotalSweepAngleDegrees, int32 SectorCount, float StartAngleOffsetDegrees, int32 SectorIndex, FRSCombatShape& OutShape, FTransform& OutShapeTransform)
{
	OutShape = FRSCombatShape();
	OutShapeTransform = FTransform::Identity;

	float SectorStartAngleDegrees = 0.0f;
	float SectorSweepAngleDegrees = 0.0f;
	if (!TryCalculateSectorAngles(TotalSweepAngleDegrees, SectorCount, StartAngleOffsetDegrees, SectorIndex, SectorStartAngleDegrees, SectorSweepAngleDegrees))
	{
		return false;
	}

	// 계산한 시작 각도와 Sweep을 그대로 담으므로 중심축 기준으로 되돌리는 변환이 필요하지 않습니다
	OutShape.Type = ERSCombatShapeType::AnnularSector;
	OutShape.OuterRadius = OuterRadius;
	OutShape.StartYawOffset = SectorStartAngleDegrees;
	OutShape.SweepAngleDegrees = SectorSweepAngleDegrees;
	if (!OutShape.IsDataValid())
	{
		return false;
	}

	// 시작 각도를 형상이 들고 있으므로 Transform은 고정된 공격 기준을 그대로 씁니다
	OutShapeTransform = LockedTransform;

	return true;
}
