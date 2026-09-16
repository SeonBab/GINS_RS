#pragma once

#include "CoreMinimal.h"

class UProgressBar;

/** 연속 레이어 좌표를 하나의 ProgressBar가 표시할 남은 레이어 수와 비율로 나눈 결과입니다 */
struct FRSBossHealthLayerDisplay
{
	/** 현재 활성 레이어를 포함한 남은 레이어 수이며 0이면 표시할 레이어가 없습니다 */
	int32 RemainingLayerCount = 0;

	/** 현재 활성 레이어 안에서의 0부터 1까지 비율입니다 */
	float LayerPercent = 0.0f;
};

namespace RSBossHealthBar
{
	/** 강도와 진행 시간에 대응하는 체력바 흔들림 위치를 반환합니다 */
	FVector2D CalculateTranslation(float Strength, float NormalizedTime, float MaxShakeX, float MaxShakeY);

	/** 하나의 ProgressBar에 현재 레이어와 다음 레이어의 색을 적용합니다 */
	void ApplyProgressBarLayerColors(UProgressBar& ProgressBar, const FLinearColor& CurrentColor, const FLinearColor& BackgroundColor);

	/**
	 * 표시용 체력을 한 프레임만큼 실제 체력 쪽으로 이동시킨 레이어 단위 연속값을 반환합니다
	 * 이동 속도가 남은 간격에 비례하므로 크게 깎일수록 빠르게 줄고 목표가 갱신되면 속도도 함께 갱신됩니다
	 */
	float AdvanceLayerScaled(float Current, float Target, float DeltaTime, float InterpSpeed, float SnapThreshold);

	/**
	 * 레이어 단위 연속값을 하나의 ProgressBar가 표시할 남은 레이어 수와 비율로 나눕니다
	 * 애니메이션 중인 표시값을 입력으로 받으며 ViewModel의 실제 체력 계산을 대신하지 않습니다
	 */
	FRSBossHealthLayerDisplay CalculateLayerDisplay(float LayerScaled, int32 LayerCount);

	/** 레이어 단위 잔상 값을 표시 중인 레이어 기준의 0부터 1까지 비율로 변환합니다 */
	float CalculateDelayedLayerPercent(float DelayedLayerScaled, int32 RemainingLayerCount);
}
