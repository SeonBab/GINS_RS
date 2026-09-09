#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Abilities/RSGameplayAbility_Knockdown.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSKnockbackTimingTest, "RS.Combat.KnockbackTiming", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSKnockbackTimingTest::RunTest(const FString& Parameters)
{
	int32 AirLoopCount = INDEX_NONE;
	float EffectiveDuration = 0.0f;

	TestTrue(TEXT("Short request uses fixed Start and Fall"), URSGameplayAbility_Knockdown::TryCalculateAirborneTiming(0.5f, 0.5f, 0.2f, 0.3f, AirLoopCount, EffectiveDuration));
	TestEqual(TEXT("Short request skips AirLoop"), AirLoopCount, 0);
	TestTrue(TEXT("Short request keeps fixed airborne duration"), FMath::IsNearlyEqual(EffectiveDuration, 0.8f));

	TestTrue(TEXT("Exact loop boundary is valid"), URSGameplayAbility_Knockdown::TryCalculateAirborneTiming(1.0f, 0.5f, 0.2f, 0.3f, AirLoopCount, EffectiveDuration));
	TestEqual(TEXT("Exact loop boundary does not add an extra loop"), AirLoopCount, 1);
	TestTrue(TEXT("Exact loop boundary keeps requested duration"), FMath::IsNearlyEqual(EffectiveDuration, 1.0f));

	TestTrue(TEXT("Partial loop request is valid"), URSGameplayAbility_Knockdown::TryCalculateAirborneTiming(1.01f, 0.5f, 0.2f, 0.3f, AirLoopCount, EffectiveDuration));
	TestEqual(TEXT("Partial loop request rounds up"), AirLoopCount, 2);
	TestTrue(TEXT("Rounded duration uses complete loops"), FMath::IsNearlyEqual(EffectiveDuration, 1.2f));

	TestFalse(TEXT("Zero loop duration is invalid"), URSGameplayAbility_Knockdown::TryCalculateAirborneTiming(1.0f, 0.5f, 0.0f, 0.3f, AirLoopCount, EffectiveDuration));
	TestEqual(TEXT("Invalid timing resets loop output"), AirLoopCount, 0);
	TestTrue(TEXT("Invalid timing resets duration output"), FMath::IsNearlyZero(EffectiveDuration));

	return true;
}

#endif
