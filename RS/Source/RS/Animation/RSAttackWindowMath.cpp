// Fill out your copyright notice in the Description page of Project Settings.

#include "RSAttackWindowMath.h"

namespace
{
	bool IsFiniteAttackWindowValue(float Value)
	{
		return FMath::IsFinite(Value);
	}
}

bool FRSAttackWindowMath::TryCalculateAlpha(float MontagePosition, float WindowStartPosition, float WindowEndPosition, float& OutAlpha)
{
	OutAlpha = 0.0f;
	if (!IsFiniteAttackWindowValue(MontagePosition) || !IsFiniteAttackWindowValue(WindowStartPosition) || !IsFiniteAttackWindowValue(WindowEndPosition) || WindowEndPosition - WindowStartPosition <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	OutAlpha = FMath::Clamp((MontagePosition - WindowStartPosition) / (WindowEndPosition - WindowStartPosition), 0.0f, 1.0f);

	return true;
}

bool FRSAttackWindowMath::TryCalculateStep(float PreviousMontagePosition, float CurrentMontagePosition, float WindowStartPosition, float WindowEndPosition, bool bWindowActive, FRSAttackWindowStep& OutStep)
{
	OutStep = FRSAttackWindowStep();
	if (!IsFiniteAttackWindowValue(PreviousMontagePosition) || !IsFiniteAttackWindowValue(CurrentMontagePosition) || CurrentMontagePosition + KINDA_SMALL_NUMBER < PreviousMontagePosition)
	{
		return false;
	}

	float PreviousAlpha = 0.0f;
	float CurrentAlpha = 0.0f;
	if (!TryCalculateAlpha(PreviousMontagePosition, WindowStartPosition, WindowEndPosition, PreviousAlpha)
		|| !TryCalculateAlpha(CurrentMontagePosition, WindowStartPosition, WindowEndPosition, CurrentAlpha))
	{
		return false;
	}

	if (!bWindowActive && CurrentMontagePosition + KINDA_SMALL_NUMBER < WindowStartPosition)
	{
		return true;
	}

	OutStep.bShouldBegin = !bWindowActive;
	OutStep.PreviousAlpha = OutStep.bShouldBegin ? 0.0f : PreviousAlpha;
	OutStep.CurrentAlpha = CurrentAlpha;
	OutStep.bShouldAdvance = OutStep.CurrentAlpha > OutStep.PreviousAlpha + KINDA_SMALL_NUMBER;
	OutStep.bShouldEnd = CurrentMontagePosition + KINDA_SMALL_NUMBER >= WindowEndPosition;

	return true;
}
