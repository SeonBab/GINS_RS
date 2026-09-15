#if WITH_DEV_AUTOMATION_TESTS

#include <limits>

#include "Misc/AutomationTest.h"
#include "Misc/DataValidation.h"

#include "Abilities/Boss/RSGameplayAbility_PizzaMemoryPattern.h"
#include "Abilities/Boss/RSPizzaMemoryPatternDefinition.h"
#include "Combat/RSCircularSliceMath.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSPizzaMemoryPatternDefinitionTest, "RS.Combat.PizzaMemoryPattern.Definition", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSPizzaMemoryPatternDefinitionTest::RunTest(const FString& Parameters)
{
	FRSPizzaMemoryPatternDefinition Definition;
	TestTrue(TEXT("Default definition is valid"), Definition.IsDataValid());
	TestEqual(TEXT("Default sequence length is four"), Definition.GetSequenceLength(), 4);

	const TArray<TPair<int32, int32>> ExpectedSafeSlices = {
		{ 0, 4 },
		{ 1, 5 },
		{ 2, 6 },
		{ 3, 7 }
	};

	for (int32 SafePairIndex = 0; SafePairIndex < ExpectedSafeSlices.Num(); ++SafePairIndex)
	{
		int32 FirstSliceIndex = INDEX_NONE;
		int32 SecondSliceIndex = INDEX_NONE;
		TestTrue(FString::Printf(TEXT("Safe pair %d is valid"), SafePairIndex), FRSPizzaMemoryPatternDefinition::TryGetSafeSliceIndices(static_cast<ERSPizzaMemorySafePair>(SafePairIndex), FirstSliceIndex, SecondSliceIndex));
		TestEqual(FString::Printf(TEXT("Safe pair %d first slice"), SafePairIndex), FirstSliceIndex, ExpectedSafeSlices[SafePairIndex].Key);
		TestEqual(FString::Printf(TEXT("Safe pair %d second slice"), SafePairIndex), SecondSliceIndex, ExpectedSafeSlices[SafePairIndex].Value);
		TestTrue(FString::Printf(TEXT("Safe pair %d slices are opposite"), SafePairIndex), RSCircularSliceMath::AreSlicesOpposite(FRSPizzaMemoryPatternDefinition::SliceCount, FirstSliceIndex, SecondSliceIndex));
	}

	FRSPizzaMemorySafeZoneSequence RepeatedSequence;
	RepeatedSequence.SafePairs = {
		ERSPizzaMemorySafePair::Slice3And7,
		ERSPizzaMemorySafePair::Slice3And7,
		ERSPizzaMemorySafePair::Slice0And4,
		ERSPizzaMemorySafePair::Slice3And7
	};
	Definition.SafeZoneSequenceCandidates.Add(RepeatedSequence);
	TestTrue(TEXT("Consecutive and non-consecutive repeats are valid"), Definition.IsDataValid());

	FRSPizzaMemoryPatternDefinition EmptyPoolDefinition;
	EmptyPoolDefinition.SafeZoneSequenceCandidates.Reset();
	TestFalse(TEXT("Empty candidate pool is invalid"), EmptyPoolDefinition.IsDataValid());

	FRSPizzaMemoryPatternDefinition EmptySequenceDefinition;
	EmptySequenceDefinition.SafeZoneSequenceCandidates[0].SafePairs.Reset();
	TestFalse(TEXT("Empty sequence is invalid"), EmptySequenceDefinition.IsDataValid());

	FRSPizzaMemoryPatternDefinition DifferentLengthDefinition;
	FRSPizzaMemorySafeZoneSequence ShortSequence;
	ShortSequence.SafePairs = { ERSPizzaMemorySafePair::Slice0And4 };
	DifferentLengthDefinition.SafeZoneSequenceCandidates.Add(ShortSequence);
	TestFalse(TEXT("Candidates with different lengths are invalid"), DifferentLengthDefinition.IsDataValid());

	FRSPizzaMemoryPatternDefinition InvalidPairDefinition;
	InvalidPairDefinition.SafeZoneSequenceCandidates[0].SafePairs[0] = static_cast<ERSPizzaMemorySafePair>(FRSPizzaMemoryPatternDefinition::SafePairCount);
	TestFalse(TEXT("Unknown safe pair is invalid"), InvalidPairDefinition.IsDataValid());

	FRSPizzaMemoryPatternDefinition InvalidOuterRadiusDefinition;
	InvalidOuterRadiusDefinition.OuterRadius = 0.0f;
	TestFalse(TEXT("Zero outer radius is invalid"), InvalidOuterRadiusDefinition.IsDataValid());

	FRSPizzaMemoryPatternDefinition InvalidAttackStartDelayDefinition;
	InvalidAttackStartDelayDefinition.AttackStartDelay = -0.1f;
	TestFalse(TEXT("Negative attack start delay is invalid"), InvalidAttackStartDelayDefinition.IsDataValid());

	FRSPizzaMemoryPatternDefinition InvalidMemoryCueDurationDefinition;
	InvalidMemoryCueDurationDefinition.MemoryCueDuration = 0.0f;
	TestFalse(TEXT("Zero memory cue duration is invalid"), InvalidMemoryCueDurationDefinition.IsDataValid());

	FRSPizzaMemoryPatternDefinition InvalidMemoryCueGapDefinition;
	InvalidMemoryCueGapDefinition.MemoryCueGap = 0.0f;
	TestFalse(TEXT("Zero memory cue gap is invalid"), InvalidMemoryCueGapDefinition.IsDataValid());

	FRSPizzaMemoryPatternDefinition InvalidRecallDelayDefinition;
	InvalidRecallDelayDefinition.RecallDelay = -0.1f;
	TestFalse(TEXT("Negative recall delay is invalid"), InvalidRecallDelayDefinition.IsDataValid());

	FRSPizzaMemoryPatternDefinition InvalidExplosionIntervalDefinition;
	InvalidExplosionIntervalDefinition.ExplosionInterval = 0.0f;
	TestFalse(TEXT("Zero explosion interval is invalid"), InvalidExplosionIntervalDefinition.IsDataValid());

	FRSPizzaMemoryPatternDefinition NonFiniteTimingDefinition;
	NonFiniteTimingDefinition.RecallDelay = std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("Non-finite timing is invalid"), NonFiniteTimingDefinition.IsDataValid());

	FRSPizzaMemoryPatternDefinition ZeroDelayDefinition;
	ZeroDelayDefinition.AttackStartDelay = 0.0f;
	ZeroDelayDefinition.RecallDelay = 0.0f;
	TestTrue(TEXT("Zero attack start and recall delays are valid"), ZeroDelayDefinition.IsDataValid());

	int32 FirstSliceIndex = 0;
	int32 SecondSliceIndex = 0;
	TestFalse(TEXT("Unknown safe pair cannot produce slice indices"), FRSPizzaMemoryPatternDefinition::TryGetSafeSliceIndices(static_cast<ERSPizzaMemorySafePair>(FRSPizzaMemoryPatternDefinition::SafePairCount), FirstSliceIndex, SecondSliceIndex));
	TestEqual(TEXT("Invalid safe pair resets first output"), FirstSliceIndex, INDEX_NONE);
	TestEqual(TEXT("Invalid safe pair resets second output"), SecondSliceIndex, INDEX_NONE);

	const UObject* AbilityDefaultObject = GetDefault<URSGameplayAbility_PizzaMemoryPattern>();
	FDataValidationContext ValidationContext;
	TestEqual(TEXT("Default Ability data validation succeeds"), AbilityDefaultObject->IsDataValid(ValidationContext), EDataValidationResult::Valid);
	TestEqual(TEXT("Default Ability data validation has no errors"), ValidationContext.GetNumErrors(), uint32{ 0 });

	return true;
}

#endif
