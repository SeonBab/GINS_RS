#pragma once

#include "CoreMinimal.h"

class UHorizontalBox;

namespace RSFireballCharge
{
	/**
	 * 체력 칸 컨테이너의 칸 수를 필요 타격 수에 맞추고 남은 타격 수만큼 왼쪽 칸부터 채웁니다
	 * 칸 수가 그대로면 기존 칸의 색만 바꾸므로 피격마다 칸을 다시 만들지 않습니다
	 */
	void ApplyHitPointPips(UHorizontalBox& PipBox, int32 CurrentHitPoints, int32 MaxHitPoints, const FLinearColor& FilledColor, const FLinearColor& EmptyColor, float PipSpacing);
}
