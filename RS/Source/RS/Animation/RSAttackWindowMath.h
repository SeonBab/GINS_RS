// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/** 한 Tick의 Montage Position 변화에서 발생할 Attack Window 상태입니다 */
struct FRSAttackWindowStep
{
	float PreviousAlpha = 0.0f;
	float CurrentAlpha = 0.0f;
	bool bShouldBegin = false;
	bool bShouldAdvance = false;
	bool bShouldEnd = false;
};

/** Montage의 Attack Window 진행률 계산을 제공합니다 */
struct RS_API FRSAttackWindowMath
{
	/** Montage Position을 Attack Window의 0~1 진행률로 변환합니다 */
	static bool TryCalculateAlpha(float MontagePosition, float WindowStartPosition, float WindowEndPosition, float& OutAlpha);

	/** 연속된 Montage Position 사이에서 Attack Window의 Begin·진행·End 상태를 계산합니다 */
	static bool TryCalculateStep(float PreviousMontagePosition, float CurrentMontagePosition, float WindowStartPosition, float WindowEndPosition, bool bWindowActive, FRSAttackWindowStep& OutStep);
};
