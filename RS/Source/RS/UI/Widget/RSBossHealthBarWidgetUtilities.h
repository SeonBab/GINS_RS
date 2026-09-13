#pragma once

#include "CoreMinimal.h"

class UProgressBar;

namespace RSBossHealthBarShake
{
	/** 강도와 진행 시간에 대응하는 체력바 흔들림 위치를 반환합니다 */
	FVector2D CalculateTranslation(float Strength, float NormalizedTime, float MaxShakeX, float MaxShakeY);

	/** 하나의 ProgressBar에 현재 레이어와 다음 레이어의 색을 적용합니다 */
	void ApplyProgressBarLayerColors(UProgressBar& ProgressBar, const FLinearColor& CurrentColor, const FLinearColor& BackgroundColor);
}
