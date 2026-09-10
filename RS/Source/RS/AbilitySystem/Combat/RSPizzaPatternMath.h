#pragma once

#include "CoreMinimal.h"

/** 피자 패턴의 가상 조각 배치와 대상 분류를 계산합니다 */
namespace RSPizzaPatternMath
{
	/** Capsule 바닥과 수평 Forward로 월드 고정 패턴 Transform을 계산합니다 */
	bool TryCalculateLockedPatternTransform(const FTransform& CapsuleTransform, float CapsuleHalfHeight, const FVector& ActorForward, FTransform& OutLockedTransform);

	/** 한 폭발의 조각 수에서 가상 조각 하나의 각도를 반환하며 입력이 잘못되면 0입니다 */
	float CalculateSliceAngleDegrees(int32 SliceCount);

	/** 폭발 번호가 사용할 모든 조각의 월드 Transform을 계산합니다 */
	bool TryBuildExplosionSliceTransforms(const FTransform& LockedTransform, int32 SliceCount, int32 ExplosionIndex, TArray<FTransform>& OutSliceTransforms);

	/** 대상 위치가 속하는 유일한 가상 조각 인덱스를 계산합니다 */
	bool TryCalculateVirtualSliceIndex(const FTransform& LockedTransform, int32 SliceCount, const FVector& TargetLocation, int64& OutVirtualSliceIndex);

	/** 대상 위치가 지정한 폭발의 A 또는 B 그룹에 속하는지 반환합니다 */
	bool IsLocationInExplosionGroup(const FTransform& LockedTransform, int32 SliceCount, int32 ExplosionIndex, const FVector& TargetLocation);
}
