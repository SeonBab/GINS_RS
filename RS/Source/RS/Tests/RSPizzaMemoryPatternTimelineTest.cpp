#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Abilities/Boss/RSPizzaMemoryPatternDefinition.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSPizzaMemoryPatternTimelineTest, "RS.Combat.PizzaMemoryPattern.Timeline", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSPizzaMemoryPatternTimelineTest::RunTest(const FString& Parameters)
{
	FRSPizzaMemoryPatternDefinition Definition;
	Definition.MemoryCueDuration = 0.6f;
	Definition.MemoryCueGap = 0.25f;
	Definition.RecallDelay = 1.2f;
	Definition.ExplosionInterval = 0.8f;

	FRSPizzaMemoryPatternTimeline Timeline;
	TestEqual(TEXT("Inactive timeline has no sequence index"), Timeline.GetSequenceIndex(), INDEX_NONE);
	TestEqual(TEXT("Inactive timeline has no delay"), Timeline.GetCurrentDelaySeconds(Definition), 0.0f);
	TestFalse(TEXT("Zero-length sequence cannot initialize"), Timeline.Initialize(0));
	TestEqual(TEXT("Failed initialization keeps the timeline inactive"), Timeline.GetPhase(), ERSPizzaMemoryPatternTimelinePhase::Inactive);

	constexpr int32 SequenceLength = 4;
	TestTrue(TEXT("Four-step sequence initializes"), Timeline.Initialize(SequenceLength));

	TArray<int32> MemoryCueIndices;
	for (int32 ExpectedIndex = 0; ExpectedIndex < SequenceLength; ++ExpectedIndex)
	{
		TestEqual(FString::Printf(TEXT("Memory cue %d phase"), ExpectedIndex), Timeline.GetPhase(), ERSPizzaMemoryPatternTimelinePhase::MemoryCue);
		TestEqual(FString::Printf(TEXT("Memory cue %d index"), ExpectedIndex), Timeline.GetSequenceIndex(), ExpectedIndex);
		TestEqual(FString::Printf(TEXT("Memory cue %d duration"), ExpectedIndex), Timeline.GetCurrentDelaySeconds(Definition), Definition.MemoryCueDuration);
		MemoryCueIndices.Add(Timeline.GetSequenceIndex());
		TestTrue(FString::Printf(TEXT("Memory cue %d advances"), ExpectedIndex), Timeline.Advance());

		if (ExpectedIndex == SequenceLength - 1)
		{
			TestEqual(TEXT("Last memory cue advances directly to recall"), Timeline.GetPhase(), ERSPizzaMemoryPatternTimelinePhase::RecallDelay);
			continue;
		}

		TestEqual(FString::Printf(TEXT("Memory cue %d is followed by a gap"), ExpectedIndex), Timeline.GetPhase(), ERSPizzaMemoryPatternTimelinePhase::MemoryCueGap);
		TestEqual(FString::Printf(TEXT("Memory gap %d duration"), ExpectedIndex), Timeline.GetCurrentDelaySeconds(Definition), Definition.MemoryCueGap);
		TestTrue(FString::Printf(TEXT("Memory gap %d advances"), ExpectedIndex), Timeline.Advance());
	}

	TestEqual(TEXT("Recall delay uses the configured duration"), Timeline.GetCurrentDelaySeconds(Definition), Definition.RecallDelay);
	TestTrue(TEXT("Recall advances to the first explosion"), Timeline.Advance());

	TArray<int32> ExplosionIndices;
	for (int32 ExpectedIndex = 0; ExpectedIndex < SequenceLength; ++ExpectedIndex)
	{
		TestEqual(FString::Printf(TEXT("Explosion %d phase"), ExpectedIndex), Timeline.GetPhase(), ERSPizzaMemoryPatternTimelinePhase::Explosion);
		TestEqual(FString::Printf(TEXT("Explosion %d index"), ExpectedIndex), Timeline.GetSequenceIndex(), ExpectedIndex);
		TestEqual(FString::Printf(TEXT("Explosion %d has no pre-delay"), ExpectedIndex), Timeline.GetCurrentDelaySeconds(Definition), 0.0f);
		ExplosionIndices.Add(Timeline.GetSequenceIndex());
		TestTrue(FString::Printf(TEXT("Explosion %d advances"), ExpectedIndex), Timeline.Advance());

		if (ExpectedIndex == SequenceLength - 1)
		{
			TestEqual(TEXT("Last explosion completes the timeline"), Timeline.GetPhase(), ERSPizzaMemoryPatternTimelinePhase::Complete);
			continue;
		}

		TestEqual(FString::Printf(TEXT("Explosion %d is followed by an interval"), ExpectedIndex), Timeline.GetPhase(), ERSPizzaMemoryPatternTimelinePhase::ExplosionInterval);
		TestEqual(FString::Printf(TEXT("Explosion interval %d duration"), ExpectedIndex), Timeline.GetCurrentDelaySeconds(Definition), Definition.ExplosionInterval);
		TestTrue(FString::Printf(TEXT("Explosion interval %d advances"), ExpectedIndex), Timeline.Advance());
	}

	TestTrue(TEXT("Memory and explosion phases use the same index order"), ExplosionIndices == MemoryCueIndices);
	TestEqual(TEXT("Complete timeline has no delay"), Timeline.GetCurrentDelaySeconds(Definition), 0.0f);
	TestFalse(TEXT("Complete timeline cannot advance"), Timeline.Advance());

	Timeline.Reset();
	TestTrue(TEXT("Single-step sequence initializes"), Timeline.Initialize(1));
	TestTrue(TEXT("Single memory cue advances"), Timeline.Advance());
	TestEqual(TEXT("Single memory cue has no trailing gap"), Timeline.GetPhase(), ERSPizzaMemoryPatternTimelinePhase::RecallDelay);
	TestTrue(TEXT("Single recall advances"), Timeline.Advance());
	TestTrue(TEXT("Single explosion advances"), Timeline.Advance());
	TestEqual(TEXT("Single explosion has no trailing interval"), Timeline.GetPhase(), ERSPizzaMemoryPatternTimelinePhase::Complete);

	return true;
}

#endif
