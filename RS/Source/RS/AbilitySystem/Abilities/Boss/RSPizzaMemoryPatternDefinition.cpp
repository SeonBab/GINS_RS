#include "RSPizzaMemoryPatternDefinition.h"

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

	return true;
}

int32 FRSPizzaMemoryPatternDefinition::GetSequenceLength() const
{
	return SafeZoneSequenceCandidates.IsEmpty() ? 0 : SafeZoneSequenceCandidates[0].SafePairs.Num();
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
