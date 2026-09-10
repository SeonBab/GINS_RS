#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "RSAttackWindowMath.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSAttackWindowMathTest, "RS.Animation.AttackWindow.Math", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSAttackWindowMathTest::RunTest(const FString& Parameters)
{
	float WindowAlpha = -1.0f;
	TestTrue(TEXT("Window alpha is calculated"), FRSAttackWindowMath::TryCalculateAlpha(1.5f, 1.0f, 2.0f, WindowAlpha));
	TestTrue(TEXT("Window alpha follows montage position"), FMath::IsNearlyEqual(WindowAlpha, 0.5f));
	TestTrue(TEXT("Window alpha clamps before the window"), FRSAttackWindowMath::TryCalculateAlpha(0.5f, 1.0f, 2.0f, WindowAlpha) && FMath::IsNearlyZero(WindowAlpha));
	TestTrue(TEXT("Window alpha clamps after the window"), FRSAttackWindowMath::TryCalculateAlpha(2.5f, 1.0f, 2.0f, WindowAlpha) && FMath::IsNearlyEqual(WindowAlpha, 1.0f));
	TestFalse(TEXT("Window rejects a zero duration"), FRSAttackWindowMath::TryCalculateAlpha(1.0f, 1.0f, 1.0f, WindowAlpha));
	TestTrue(TEXT("Invalid window resets alpha"), FMath::IsNearlyZero(WindowAlpha));

	FRSAttackWindowStep BeforeWindowStep;
	TestTrue(TEXT("Position before window is valid"), FRSAttackWindowMath::TryCalculateStep(0.0f, 0.5f, 1.0f, 2.0f, false, BeforeWindowStep));
	TestFalse(TEXT("Position before window does not begin"), BeforeWindowStep.bShouldBegin);

	FRSAttackWindowStep EnterWindowStep;
	TestTrue(TEXT("Entering window is calculated"), FRSAttackWindowMath::TryCalculateStep(0.5f, 1.25f, 1.0f, 2.0f, false, EnterWindowStep));
	TestTrue(TEXT("Entering window begins"), EnterWindowStep.bShouldBegin);
	TestTrue(TEXT("Skipped window start advances from zero"), EnterWindowStep.bShouldAdvance && FMath::IsNearlyZero(EnterWindowStep.PreviousAlpha) && FMath::IsNearlyEqual(EnterWindowStep.CurrentAlpha, 0.25f));
	TestFalse(TEXT("Entering inside window does not end"), EnterWindowStep.bShouldEnd);

	FRSAttackWindowStep ActiveWindowStep;
	TestTrue(TEXT("Active window is calculated"), FRSAttackWindowMath::TryCalculateStep(1.25f, 1.75f, 1.0f, 2.0f, true, ActiveWindowStep));
	TestFalse(TEXT("Active window does not begin twice"), ActiveWindowStep.bShouldBegin);
	TestTrue(TEXT("Active window advances between alphas"), ActiveWindowStep.bShouldAdvance && FMath::IsNearlyEqual(ActiveWindowStep.PreviousAlpha, 0.25f) && FMath::IsNearlyEqual(ActiveWindowStep.CurrentAlpha, 0.75f));

	FRSAttackWindowStep EndWindowStep;
	TestTrue(TEXT("Crossing window end is calculated"), FRSAttackWindowMath::TryCalculateStep(1.75f, 2.25f, 1.0f, 2.0f, true, EndWindowStep));
	TestTrue(TEXT("Crossing window end fills the remaining range"), EndWindowStep.bShouldAdvance && FMath::IsNearlyEqual(EndWindowStep.PreviousAlpha, 0.75f) && FMath::IsNearlyEqual(EndWindowStep.CurrentAlpha, 1.0f));
	TestTrue(TEXT("Crossing window end completes"), EndWindowStep.bShouldEnd);

	FRSAttackWindowStep SkippedWindowStep;
	TestTrue(TEXT("Skipping the whole window is calculated"), FRSAttackWindowMath::TryCalculateStep(0.5f, 2.25f, 1.0f, 2.0f, false, SkippedWindowStep));
	TestTrue(TEXT("Skipping the whole window begins and ends"), SkippedWindowStep.bShouldBegin && SkippedWindowStep.bShouldAdvance && SkippedWindowStep.bShouldEnd);
	TestTrue(TEXT("Skipping the whole window covers zero to one"), FMath::IsNearlyZero(SkippedWindowStep.PreviousAlpha) && FMath::IsNearlyEqual(SkippedWindowStep.CurrentAlpha, 1.0f));

	FRSAttackWindowStep ReversedWindowStep;
	TestFalse(TEXT("Reverse montage progress is rejected"), FRSAttackWindowMath::TryCalculateStep(1.5f, 1.25f, 1.0f, 2.0f, true, ReversedWindowStep));

	return true;
}

#endif
