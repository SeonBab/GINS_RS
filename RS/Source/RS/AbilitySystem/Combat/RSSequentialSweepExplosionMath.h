#pragma once

#include "CoreMinimal.h"

/** 순차 스윕 폭발 패턴의 공간과 시간 계산을 제공합니다 */
namespace RSSequentialSweepExplosionMath
{
	/** Capsule 바닥과 수평 Forward로 월드 고정 공격 Transform을 계산합니다 */
	bool TryCalculateLockedAttackTransform(const FTransform& CapsuleTransform, float CapsuleHalfHeight, const FVector& ActorForward, FTransform& OutLockedTransform);

	/** 전체 공격 각도를 설정한 개수로 균등 분할한 부채꼴 각도를 반환합니다 */
	float CalculateSectorAngleDegrees(float TotalSweepAngleDegrees, int32 SectorCount);

	/** 첫 폭발부터 마지막 폭발까지 포함하는 전체 공격 타임라인 길이를 반환합니다 */
	float CalculateTotalDuration(float FirstExplosionDelay, float SectorExplosionInterval, int32 SectorCount);

	/** 경과 시간까지 누적 표시돼야 할 부채꼴 개수를 반환합니다 */
	int32 CalculateRequiredWarningCount(float ElapsedTime, float FirstExplosionDelay, int32 SectorCount);

	/** 경과 시간까지 폭발해야 할 부채꼴 개수를 반환합니다 */
	int32 CalculateRequiredExplosionCount(float ElapsedTime, float FirstExplosionDelay, float SectorExplosionInterval, int32 SectorCount);

	/** 지정한 부채꼴의 시작 Offset과 시계 방향 Sweep 각도를 계산합니다 */
	bool TryCalculateSectorAngles(float TotalSweepAngleDegrees, int32 SectorCount, float StartAngleOffsetDegrees, int32 SectorIndex, float& OutStartAngleOffsetDegrees, float& OutSweepAngleDegrees);

	/** 대상 위치가 바깥 반경과 양쪽 각도 경계를 포함한 지정 부채꼴에 속하는지 반환합니다 */
	bool IsLocationInSector(const FTransform& LockedTransform, float OuterRadius, float TotalSweepAngleDegrees, int32 SectorCount, float StartAngleOffsetDegrees, int32 SectorIndex, const FVector& TargetLocation);
}
