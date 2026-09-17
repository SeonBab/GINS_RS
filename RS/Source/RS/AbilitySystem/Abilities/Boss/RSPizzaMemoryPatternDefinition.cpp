#include "RSPizzaMemoryPatternDefinition.h"

#include "Combat/RSCircularSliceMath.h"
#include "Combat/RSCombatFunctionLibrary.h"

FRSPizzaMemoryPatternDefinition::FRSPizzaMemoryPatternDefinition()
{
	FRSPizzaMemorySafeZoneSequence& DefaultSequence = SafeZoneSequenceCandidates.AddDefaulted_GetRef();
	DefaultSequence.SafePairs = {
		ERSPizzaMemorySafePair::Slice0And4,
		ERSPizzaMemorySafePair::Slice1And5,
		ERSPizzaMemorySafePair::Slice2And6,
		ERSPizzaMemorySafePair::Slice3And7
	};
}

bool FRSPizzaMemoryPatternDefinition::IsDataValid(FString* OutValidationError) const
{
	if (OutValidationError)
	{
		OutValidationError->Reset();
	}

	auto FailValidation = [OutValidationError](const FString& ErrorMessage)
	{
		if (OutValidationError)
		{
			*OutValidationError = ErrorMessage;
		}

		return false;
	};

	if (SafeZoneSequenceCandidates.IsEmpty())
	{
		return FailValidation(TEXT("SafeZoneSequenceCandidates must contain at least one sequence."));
	}

	const int32 SequenceLength = SafeZoneSequenceCandidates[0].SafePairs.Num();
	if (SequenceLength <= 0)
	{
		return FailValidation(TEXT("SafeZoneSequenceCandidates[0].SafePairs must not be empty."));
	}

	for (int32 CandidateIndex = 0; CandidateIndex < SafeZoneSequenceCandidates.Num(); ++CandidateIndex)
	{
		const TArray<ERSPizzaMemorySafePair>& SafePairs = SafeZoneSequenceCandidates[CandidateIndex].SafePairs;
		if (SafePairs.Num() != SequenceLength)
		{
			return FailValidation(FString::Printf(TEXT("SafeZoneSequenceCandidates[%d].SafePairs has %d entries but every sequence must have %d."), CandidateIndex, SafePairs.Num(), SequenceLength));
		}

		for (int32 SequenceIndex = 0; SequenceIndex < SafePairs.Num(); ++SequenceIndex)
		{
			int32 FirstSliceIndex = INDEX_NONE;
			int32 SecondSliceIndex = INDEX_NONE;
			if (!TryGetSafeSliceIndices(SafePairs[SequenceIndex], FirstSliceIndex, SecondSliceIndex))
			{
				return FailValidation(FString::Printf(TEXT("SafeZoneSequenceCandidates[%d].SafePairs[%d] is not a valid safe pair."), CandidateIndex, SequenceIndex));
			}
		}
	}

	if (!FMath::IsFinite(OuterRadius) || OuterRadius <= 0.0f)
	{
		return FailValidation(TEXT("OuterRadius must be finite and greater than zero."));
	}

	if (!FMath::IsFinite(InnerRadius) || InnerRadius < 0.0f || InnerRadius >= OuterRadius)
	{
		return FailValidation(TEXT("InnerRadius must be finite and satisfy 0 <= InnerRadius < OuterRadius."));
	}

	if (!FMath::IsFinite(AttackStartDelay) || AttackStartDelay < 0.0f)
	{
		return FailValidation(TEXT("AttackStartDelay must be finite and non-negative."));
	}

	if (!FMath::IsFinite(MemoryCueDuration) || MemoryCueDuration <= 0.0f)
	{
		return FailValidation(TEXT("MemoryCueDuration must be finite and greater than zero."));
	}

	if (!FMath::IsFinite(MemoryCueGap) || MemoryCueGap <= 0.0f)
	{
		return FailValidation(TEXT("MemoryCueGap must be finite and greater than zero so consecutive repeated cues remain distinguishable."));
	}

	if (!FMath::IsFinite(RecallDelay) || RecallDelay < 0.0f)
	{
		return FailValidation(TEXT("RecallDelay must be finite and non-negative."));
	}

	if (!FMath::IsFinite(ExplosionInterval) || ExplosionInterval <= 0.0f)
	{
		return FailValidation(TEXT("ExplosionInterval must be finite and greater than zero."));
	}

	const float DamageValue = Damage.GetValueAtLevel(1.0f);
	constexpr float DamageIntegerTolerance = 0.01f;
	if (!FMath::IsFinite(DamageValue) || DamageValue < 0.0f || !FMath::IsNearlyEqual(DamageValue, FMath::RoundToFloat(DamageValue), DamageIntegerTolerance))
	{
		return FailValidation(TEXT("Damage must evaluate to a finite non-negative integer at level 1."));
	}

	return true;
}

int32 FRSPizzaMemoryPatternDefinition::GetSequenceLength() const
{
	return SafeZoneSequenceCandidates.IsEmpty() ? 0 : SafeZoneSequenceCandidates[0].SafePairs.Num();
}

FRSPizzaCueTimings FRSPizzaMemoryPatternDefinition::MakeCueTimings() const
{
	FRSPizzaCueTimings CueTimings;
	CueTimings.CueDuration = MemoryCueDuration;
	CueTimings.CueGap = MemoryCueGap;
	CueTimings.RecallDelay = RecallDelay;
	CueTimings.ExplosionInterval = ExplosionInterval;

	return CueTimings;
}

bool FRSPizzaMemoryPatternDefinition::TryCopySafeZoneSequenceCandidate(int32 CandidateIndex, TArray<ERSPizzaMemorySafePair>& OutSafePairs) const
{
	OutSafePairs.Reset();
	if (!SafeZoneSequenceCandidates.IsValidIndex(CandidateIndex))
	{
		return false;
	}

	const TArray<ERSPizzaMemorySafePair>& CandidateSafePairs = SafeZoneSequenceCandidates[CandidateIndex].SafePairs;
	if (CandidateSafePairs.IsEmpty())
	{
		return false;
	}

	OutSafePairs = CandidateSafePairs;

	return true;
}

float FRSPizzaMemoryPatternDefinition::CalculateSliceAngleDegrees() const
{
	return RSCircularSliceMath::CalculateSliceAngleDegrees(SliceCount);
}

bool FRSPizzaMemoryPatternDefinition::TryMakeSliceShape(FRSCombatShape& OutSliceShape) const
{
	OutSliceShape = FRSCombatShape();

	// 조각 Transform이 조각 중심을 향하므로 첫 경계를 조각 각도의 절반만큼 뒤로 물립니다
	const float SliceAngleDegrees = CalculateSliceAngleDegrees();
	OutSliceShape.Type = ERSCombatShapeType::AnnularSector;
	OutSliceShape.InnerRadius = InnerRadius;
	OutSliceShape.OuterRadius = OuterRadius;
	OutSliceShape.StartYawOffset = -SliceAngleDegrees * 0.5f;
	OutSliceShape.SweepAngleDegrees = SliceAngleDegrees;

	return OutSliceShape.IsDataValid();
}

bool FRSPizzaMemoryPatternDefinition::TryGetSafeSliceIndices(ERSPizzaMemorySafePair SafePair, int32& OutFirstSliceIndex, int32& OutSecondSliceIndex)
{
	OutFirstSliceIndex = INDEX_NONE;
	OutSecondSliceIndex = INDEX_NONE;

	const int32 SafePairIndex = static_cast<int32>(SafePair);
	if (SafePairIndex < 0 || SafePairIndex >= SafePairCount)
	{
		return false;
	}

	OutFirstSliceIndex = SafePairIndex;
	OutSecondSliceIndex = SafePairIndex + SafePairCount;

	return true;
}

bool FRSPizzaMemoryPatternDefinition::TryBuildDangerousSliceTransforms(const FTransform& LockedTransform, ERSPizzaMemorySafePair SafePair, TArray<FTransform>& OutDangerousSliceTransforms) const
{
	OutDangerousSliceTransforms.Reset();

	int32 FirstSafeSliceIndex = INDEX_NONE;
	int32 SecondSafeSliceIndex = INDEX_NONE;
	if (!TryGetSafeSliceIndices(SafePair, FirstSafeSliceIndex, SecondSafeSliceIndex))
	{
		return false;
	}

	OutDangerousSliceTransforms.Reserve(SliceCount - 2);
	for (int32 SliceIndex = 0; SliceIndex < SliceCount; ++SliceIndex)
	{
		if (SliceIndex == FirstSafeSliceIndex || SliceIndex == SecondSafeSliceIndex)
		{
			continue;
		}

		FTransform SliceTransform;
		if (!RSCircularSliceMath::TryBuildSliceTransform(LockedTransform, SliceCount, SliceIndex, SliceTransform))
		{
			OutDangerousSliceTransforms.Reset();

			return false;
		}

		OutDangerousSliceTransforms.Add(SliceTransform);
	}

	return OutDangerousSliceTransforms.Num() == SliceCount - 2;
}

bool FRSPizzaMemoryPatternDefinition::TryIsLocationInSafePair(const FTransform& LockedTransform, ERSPizzaMemorySafePair SafePair, const FVector& TargetLocation, bool& OutIsSafe) const
{
	OutIsSafe = false;

	int32 FirstSafeSliceIndex = INDEX_NONE;
	int32 SecondSafeSliceIndex = INDEX_NONE;
	if (!TryGetSafeSliceIndices(SafePair, FirstSafeSliceIndex, SecondSafeSliceIndex))
	{
		return false;
	}

	int32 TargetSliceIndex = INDEX_NONE;
	if (!RSCircularSliceMath::TryCalculateSliceIndex(LockedTransform, SliceCount, TargetLocation, TargetSliceIndex))
	{
		return false;
	}

	OutIsSafe = TargetSliceIndex == FirstSafeSliceIndex || TargetSliceIndex == SecondSafeSliceIndex;

	return true;
}
