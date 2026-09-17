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

	const FRSPizzaCueTimings CueTimings = Definition.MakeCueTimings();
	TestEqual(TEXT("Cue timings use the memory cue duration"), CueTimings.CueDuration, Definition.MemoryCueDuration);
	TestEqual(TEXT("Cue timings use the memory cue gap"), CueTimings.CueGap, Definition.MemoryCueGap);
	TestEqual(TEXT("Cue timings use the recall delay"), CueTimings.RecallDelay, Definition.RecallDelay);
	TestEqual(TEXT("Cue timings use the explosion interval"), CueTimings.ExplosionInterval, Definition.ExplosionInterval);

	// 예고, 판정과 연출이 같은 조각을 쓰므로 안쪽 경계도 이 한 곳에서 실립니다
	FRSCombatShape SliceShape;
	TestTrue(TEXT("A slice shape is built"), Definition.TryMakeSliceShape(SliceShape));
	TestEqual(TEXT("A slice shape without an inner radius starts at the pattern origin"), SliceShape.InnerRadius, 0.0f);
	TestEqual(TEXT("A slice shape uses the authored outer radius"), SliceShape.OuterRadius, Definition.OuterRadius);
	TestEqual(TEXT("A slice shape spans one slice"), SliceShape.SweepAngleDegrees, Definition.CalculateSliceAngleDegrees());

	// 에셋이 적은 안쪽 경계가 그대로 실려야 에디터에 적은 값과 실제 판정 범위가 같습니다
	FRSPizzaMemoryPatternDefinition InnerRadiusDefinition = Definition;
	InnerRadiusDefinition.InnerRadius = 120.0f;
	FRSCombatShape InnerRadiusSliceShape;
	TestTrue(TEXT("A slice shape with an authored inner radius is built"), InnerRadiusDefinition.TryMakeSliceShape(InnerRadiusSliceShape));
	TestEqual(TEXT("A slice shape carries the authored inner radius"), InnerRadiusSliceShape.InnerRadius, 120.0f);
	TestEqual(TEXT("A slice shape with an inner radius keeps its angle"), InnerRadiusSliceShape.SweepAngleDegrees, Definition.CalculateSliceAngleDegrees());

	// 안쪽 경계가 바깥을 삼키면 판정할 면적이 없으므로 편집 시점과 런타임이 모두 그 사실을 알아야 합니다
	FRSPizzaMemoryPatternDefinition SwallowedDefinition = Definition;
	SwallowedDefinition.InnerRadius = Definition.OuterRadius;
	FRSCombatShape SwallowedSliceShape;
	TestFalse(TEXT("An inner radius at the outer radius leaves no slice"), SwallowedDefinition.TryMakeSliceShape(SwallowedSliceShape));
	TestFalse(TEXT("An inner radius at the outer radius fails validation"), SwallowedDefinition.IsDataValid());

	TArray<ERSPizzaMemorySafePair> CopiedSafePairs;
	TestTrue(TEXT("Default candidate can be copied"), Definition.TryCopySafeZoneSequenceCandidate(0, CopiedSafePairs));
	TestEqual(TEXT("Copied candidate keeps the sequence length"), CopiedSafePairs.Num(), Definition.GetSequenceLength());
	if (CopiedSafePairs.IsValidIndex(0) && Definition.SafeZoneSequenceCandidates[0].SafePairs.IsValidIndex(0))
	{
		const ERSPizzaMemorySafePair CopiedFirstSafePair = CopiedSafePairs[0];
		Definition.SafeZoneSequenceCandidates[0].SafePairs[0] = ERSPizzaMemorySafePair::Slice3And7;
		TestEqual(TEXT("Copied candidate is frozen independently from its source"), CopiedSafePairs[0], CopiedFirstSafePair);
	}
	else
	{
		AddError(TEXT("Default candidate does not contain a first safe pair"));
	}
	Definition = FRSPizzaMemoryPatternDefinition();

	CopiedSafePairs = { ERSPizzaMemorySafePair::Slice0And4 };
	TestFalse(TEXT("Unknown candidate cannot be copied"), Definition.TryCopySafeZoneSequenceCandidate(INDEX_NONE, CopiedSafePairs));
	TestTrue(TEXT("Failed candidate copy clears output"), CopiedSafePairs.IsEmpty());

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

		const ERSPizzaMemorySafePair SafePair = static_cast<ERSPizzaMemorySafePair>(SafePairIndex);
		const FTransform LockedTransform(FRotator::ZeroRotator, FVector::ZeroVector);
		TArray<FTransform> DangerousSliceTransforms;
		TestTrue(FString::Printf(TEXT("Safe pair %d builds dangerous transforms"), SafePairIndex), Definition.TryBuildDangerousSliceTransforms(LockedTransform, SafePair, DangerousSliceTransforms));
		TestEqual(FString::Printf(TEXT("Safe pair %d has six dangerous transforms"), SafePairIndex), DangerousSliceTransforms.Num(), 6);

		int32 DangerousTransformIndex = 0;
		int32 SafeLocationCount = 0;
		int32 DangerousLocationCount = 0;
		for (int32 SliceIndex = 0; SliceIndex < FRSPizzaMemoryPatternDefinition::SliceCount; ++SliceIndex)
		{
			const bool bExpectedSafe = SliceIndex == FirstSliceIndex || SliceIndex == SecondSliceIndex;
			const FVector SliceCenterLocation = FRotator(0.0f, SliceIndex * Definition.CalculateSliceAngleDegrees(), 0.0f).Vector() * 100.0f;
			bool bIsSafe = false;
			TestTrue(FString::Printf(TEXT("Safe pair %d classifies slice %d"), SafePairIndex, SliceIndex), Definition.TryIsLocationInSafePair(LockedTransform, SafePair, SliceCenterLocation, bIsSafe));
			TestEqual(FString::Printf(TEXT("Safe pair %d slice %d classification"), SafePairIndex, SliceIndex), bIsSafe, bExpectedSafe);

			if (bIsSafe)
			{
				++SafeLocationCount;

				continue;
			}

			++DangerousLocationCount;
			if (DangerousSliceTransforms.IsValidIndex(DangerousTransformIndex))
			{
				TestTrue(FString::Printf(TEXT("Safe pair %d dangerous transform %d yaw"), SafePairIndex, DangerousTransformIndex), FMath::IsNearlyZero(FMath::FindDeltaAngleDegrees(DangerousSliceTransforms[DangerousTransformIndex].Rotator().Yaw, SliceIndex * Definition.CalculateSliceAngleDegrees())));
			}
			else
			{
				AddError(FString::Printf(TEXT("Safe pair %d is missing dangerous transform %d"), SafePairIndex, DangerousTransformIndex));
			}
			++DangerousTransformIndex;
		}

		TestEqual(FString::Printf(TEXT("Safe pair %d has two safe locations"), SafePairIndex), SafeLocationCount, 2);
		TestEqual(FString::Printf(TEXT("Safe pair %d has six dangerous locations"), SafePairIndex), DangerousLocationCount, 6);
	}

	TestEqual(TEXT("Eight slices use forty-five degree angles"), Definition.CalculateSliceAngleDegrees(), 45.0f);

	TArray<FTransform> InvalidDangerousSliceTransforms = { FTransform::Identity };
	TestFalse(TEXT("Unknown safe pair cannot build dangerous transforms"), Definition.TryBuildDangerousSliceTransforms(FTransform::Identity, static_cast<ERSPizzaMemorySafePair>(FRSPizzaMemoryPatternDefinition::SafePairCount), InvalidDangerousSliceTransforms));
	TestTrue(TEXT("Failed dangerous transform build clears output"), InvalidDangerousSliceTransforms.IsEmpty());

	bool bIsSafe = true;
	TestFalse(TEXT("Invalid locked transform cannot classify a location"), Definition.TryIsLocationInSafePair(FTransform(FVector(std::numeric_limits<float>::quiet_NaN())), ERSPizzaMemorySafePair::Slice0And4, FVector::XAxisVector, bIsSafe));
	TestFalse(TEXT("Failed safe classification resets output"), bIsSafe);

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
	TestEqual(TEXT("Unconfigured Ability data validation fails"), AbilityDefaultObject->IsDataValid(ValidationContext), EDataValidationResult::Invalid);
	TestTrue(TEXT("Unconfigured Ability reports the missing DamageEffectClass"), ValidationContext.GetNumErrors() > 0);

	return true;
}

#endif
