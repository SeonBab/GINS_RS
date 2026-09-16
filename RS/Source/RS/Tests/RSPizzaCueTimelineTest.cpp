#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Abilities/Boss/RSPizzaCueTimeline.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSPizzaCueTimelineTest, "RS.Combat.Pizza.CueTimeline", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSPizzaCueTimelineTest::RunTest(const FString& Parameters)
{
	FRSPizzaCueTimings CueTimings;
	CueTimings.CueDuration = 0.6f;
	CueTimings.CueGap = 0.25f;
	CueTimings.RecallDelay = 1.2f;
	CueTimings.ExplosionInterval = 0.8f;

	FRSPizzaCueTimeline Timeline;
	TestEqual(TEXT("Inactive timeline has no sequence index"), Timeline.GetSequenceIndex(), INDEX_NONE);
	TestEqual(TEXT("Inactive timeline has no delay"), Timeline.GetCurrentDelaySeconds(CueTimings), 0.0f);
	TestFalse(TEXT("Zero-length sequence cannot initialize"), Timeline.Initialize(0));
	TestEqual(TEXT("Failed initialization keeps the timeline inactive"), Timeline.GetPhase(), ERSPizzaCueTimelinePhase::Inactive);

	constexpr int32 SequenceLength = 4;
	TestTrue(TEXT("Four-step sequence initializes"), Timeline.Initialize(SequenceLength));

	TArray<int32> CueIndices;
	for (int32 ExpectedIndex = 0; ExpectedIndex < SequenceLength; ++ExpectedIndex)
	{
		TestEqual(FString::Printf(TEXT("Cue %d phase"), ExpectedIndex), Timeline.GetPhase(), ERSPizzaCueTimelinePhase::Cue);
		TestEqual(FString::Printf(TEXT("Cue %d index"), ExpectedIndex), Timeline.GetSequenceIndex(), ExpectedIndex);
		TestEqual(FString::Printf(TEXT("Cue %d duration"), ExpectedIndex), Timeline.GetCurrentDelaySeconds(CueTimings), CueTimings.CueDuration);
		CueIndices.Add(Timeline.GetSequenceIndex());
		TestTrue(FString::Printf(TEXT("Cue %d advances"), ExpectedIndex), Timeline.Advance());

		if (ExpectedIndex == SequenceLength - 1)
		{
			TestEqual(TEXT("Last cue advances directly to recall"), Timeline.GetPhase(), ERSPizzaCueTimelinePhase::RecallDelay);
			continue;
		}

		TestEqual(FString::Printf(TEXT("Cue %d is followed by a gap"), ExpectedIndex), Timeline.GetPhase(), ERSPizzaCueTimelinePhase::CueGap);
		TestEqual(FString::Printf(TEXT("Cue gap %d duration"), ExpectedIndex), Timeline.GetCurrentDelaySeconds(CueTimings), CueTimings.CueGap);
		TestTrue(FString::Printf(TEXT("Cue gap %d advances"), ExpectedIndex), Timeline.Advance());
	}

	TestEqual(TEXT("Recall delay uses the configured duration"), Timeline.GetCurrentDelaySeconds(CueTimings), CueTimings.RecallDelay);
	TestTrue(TEXT("Recall advances to the first explosion"), Timeline.Advance());

	TArray<int32> ExplosionIndices;
	for (int32 ExpectedIndex = 0; ExpectedIndex < SequenceLength; ++ExpectedIndex)
	{
		TestEqual(FString::Printf(TEXT("Explosion %d phase"), ExpectedIndex), Timeline.GetPhase(), ERSPizzaCueTimelinePhase::Explosion);
		TestEqual(FString::Printf(TEXT("Explosion %d index"), ExpectedIndex), Timeline.GetSequenceIndex(), ExpectedIndex);
		TestEqual(FString::Printf(TEXT("Explosion %d has no pre-delay"), ExpectedIndex), Timeline.GetCurrentDelaySeconds(CueTimings), 0.0f);
		ExplosionIndices.Add(Timeline.GetSequenceIndex());
		TestTrue(FString::Printf(TEXT("Explosion %d advances"), ExpectedIndex), Timeline.Advance());

		if (ExpectedIndex == SequenceLength - 1)
		{
			TestEqual(TEXT("Last explosion completes the timeline"), Timeline.GetPhase(), ERSPizzaCueTimelinePhase::Complete);
			continue;
		}

		TestEqual(FString::Printf(TEXT("Explosion %d is followed by an interval"), ExpectedIndex), Timeline.GetPhase(), ERSPizzaCueTimelinePhase::ExplosionInterval);
		TestEqual(FString::Printf(TEXT("Explosion interval %d duration"), ExpectedIndex), Timeline.GetCurrentDelaySeconds(CueTimings), CueTimings.ExplosionInterval);
		TestTrue(FString::Printf(TEXT("Explosion interval %d advances"), ExpectedIndex), Timeline.Advance());
	}

	TestTrue(TEXT("Cue and explosion phases use the same index order"), ExplosionIndices == CueIndices);
	TestEqual(TEXT("Complete timeline has no delay"), Timeline.GetCurrentDelaySeconds(CueTimings), 0.0f);
	TestFalse(TEXT("Complete timeline cannot advance"), Timeline.Advance());

	// 1회형 피자 패턴은 간격 없이 예고 하나와 폭발 하나만 사용합니다
	Timeline.Reset();
	TestTrue(TEXT("Single-step sequence initializes"), Timeline.Initialize(1));
	TestTrue(TEXT("Single cue advances"), Timeline.Advance());
	TestEqual(TEXT("Single cue has no trailing gap"), Timeline.GetPhase(), ERSPizzaCueTimelinePhase::RecallDelay);
	TestTrue(TEXT("Single recall advances"), Timeline.Advance());
	TestTrue(TEXT("Single explosion advances"), Timeline.Advance());
	TestEqual(TEXT("Single explosion has no trailing interval"), Timeline.GetPhase(), ERSPizzaCueTimelinePhase::Complete);

	// 2회형 피자 패턴은 A와 B를 모두 예고한 뒤 같은 순서로 폭발합니다
	Timeline.Reset();
	TestTrue(TEXT("Two-step sequence initializes"), Timeline.Initialize(2));
	TestEqual(TEXT("Two-step sequence starts on the first cue"), Timeline.GetSequenceIndex(), 0);
	TestTrue(TEXT("First cue advances"), Timeline.Advance());
	TestEqual(TEXT("First cue is followed by a gap"), Timeline.GetPhase(), ERSPizzaCueTimelinePhase::CueGap);
	TestTrue(TEXT("Cue gap advances"), Timeline.Advance());
	TestEqual(TEXT("Second cue uses the next index"), Timeline.GetSequenceIndex(), 1);
	TestTrue(TEXT("Second cue advances"), Timeline.Advance());
	TestEqual(TEXT("Second cue advances to recall"), Timeline.GetPhase(), ERSPizzaCueTimelinePhase::RecallDelay);
	TestTrue(TEXT("Recall advances"), Timeline.Advance());
	TestEqual(TEXT("First explosion repeats the first cue index"), Timeline.GetSequenceIndex(), 0);
	TestTrue(TEXT("First explosion advances"), Timeline.Advance());
	TestEqual(TEXT("First explosion is followed by an interval"), Timeline.GetPhase(), ERSPizzaCueTimelinePhase::ExplosionInterval);
	TestTrue(TEXT("Explosion interval advances"), Timeline.Advance());
	TestEqual(TEXT("Second explosion repeats the second cue index"), Timeline.GetSequenceIndex(), 1);
	TestTrue(TEXT("Second explosion advances"), Timeline.Advance());
	TestEqual(TEXT("Second explosion completes the timeline"), Timeline.GetPhase(), ERSPizzaCueTimelinePhase::Complete);

	return true;
}

#endif
