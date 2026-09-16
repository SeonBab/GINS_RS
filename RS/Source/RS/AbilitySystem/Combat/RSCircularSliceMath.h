#pragma once

#include "CoreMinimal.h"

/** 원 전체를 같은 각도의 조각으로 나누는 패턴이 공유할 공간 계산입니다 */
namespace RSCircularSliceMath
{
	/** Capsule 바닥과 수평 Forward로 월드 고정 원형 분할 Transform을 계산합니다 */
	bool TryCalculateLockedTransformFromCapsule(const FTransform& CapsuleTransform, float CapsuleHalfHeight, const FVector& ActorForward, FTransform& OutLockedTransform);

	/** 전체 조각 수에서 조각 하나의 각도를 반환하며 입력이 잘못되면 0입니다 */
	float CalculateSliceAngleDegrees(int32 TotalSliceCount);

	/** 지정한 조각 인덱스의 월드 Transform을 계산합니다 */
	bool TryBuildSliceTransform(const FTransform& LockedTransform, int32 TotalSliceCount, int32 SliceIndex, FTransform& OutSliceTransform);

	/** 대상 위치가 속하는 유일한 조각 인덱스를 계산합니다 */
	bool TryCalculateSliceIndex(const FTransform& LockedTransform, int32 TotalSliceCount, const FVector& TargetLocation, int32& OutSliceIndex);

	/** 두 조각이 짝수 개의 전체 조각에서 서로 반대편인지 반환합니다 */
	bool AreSlicesOpposite(int32 TotalSliceCount, int32 FirstSliceIndex, int32 SecondSliceIndex);
}
