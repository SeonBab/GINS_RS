#pragma once

#include "CoreMinimal.h"
#include "RSCombatFunctionLibrary.h"

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

	/**
	 * 대상 위치가 지정한 조각에 속하는지 반환하며 예고, 연출과 같은 형상을 만들어 공용 커널에 넘깁니다
	 * 반지름 [Inner, Outer), 각도 [Start, Start + Sweep)의 반개구간이므로 인접한 두 조각이 경계 대상을 나눠 갖지 않습니다
	 * InnerRadius는 호출자가 이번 활성화에 확정한 하한이며 그 안쪽은 판정에서 빠집니다
	 */
	bool IsLocationInSector(const FTransform& LockedTransform, float InnerRadius, float OuterRadius, float TotalSweepAngleDegrees, int32 SectorCount, float StartAngleOffsetDegrees, int32 SectorIndex, const FVector& TargetLocation);

	/**
	 * 지정한 부채꼴을 Telegraph 표시와 연출 채우기가 함께 쓸 Cone 형상과 중심 Transform으로 변환합니다
	 * 두 경로가 각자 계산하면 예고한 범위와 연출이 어긋날 수 있어 한 곳에서만 계산합니다
	 * 일반 Cone은 중심축 기준 형상이므로 시작 경계에서 조각 각도의 절반만큼 회전한 Transform을 돌려줍니다
	 * 조각 각도가 Cone이 표현할 수 있는 상한을 넘으면 실패합니다
	 */
	bool TryBuildSectorFillShape(const FTransform& LockedTransform, float OuterRadius, float TotalSweepAngleDegrees, int32 SectorCount, float StartAngleOffsetDegrees, int32 SectorIndex, FRSCombatShape& OutShape, FTransform& OutShapeTransform);
}
