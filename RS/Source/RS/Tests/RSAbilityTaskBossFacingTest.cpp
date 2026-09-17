#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/World.h"
#include "Tasks/RSAbilityTask_BossFacing.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSAbilityTaskBossFacingTest, "RS.Ability.Tasks.BossFacing", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSAbilityTaskBossFacingTest::RunTest(const FString& Parameters)
{
	bool bIsWithinYawTolerance = false;
	float YawErrorDegrees = 0.0f;
	URSAbilityTask_BossFacing::CalculateYawState(179.0f, -179.0f, 1.5f, bIsWithinYawTolerance, YawErrorDegrees);
	TestEqual(TEXT("Yaw error follows the signed shortest path across the wrap boundary"), YawErrorDegrees, 2.0f);
	TestFalse(TEXT("Error outside the tolerance does not complete Facing"), bIsWithinYawTolerance);

	URSAbilityTask_BossFacing::CalculateYawState(179.0f, -179.0f, 2.0f, bIsWithinYawTolerance, YawErrorDegrees);
	TestTrue(TEXT("Yaw tolerance includes its outer boundary"), bIsWithinYawTolerance);

	const FRotator ExactRotation = URSAbilityTask_BossFacing::MakeExactYawRotation(FRotator(10.0f, 45.0f, 20.0f), 360.0f);
	TestEqual(TEXT("Exact correction preserves Pitch"), ExactRotation.Pitch, 10.0);
	TestEqual(TEXT("Exact correction normalizes and replaces Yaw"), ExactRotation.Yaw, 0.0);
	TestEqual(TEXT("Exact correction preserves Roll"), ExactRotation.Roll, 20.0);

	const FRSBossFacingRequest FixedWorldYawRequest = FRSBossFacingRequest::MakeFixedWorldYaw(0.0f, 150.0f, 0.5f, 1.5f);
	TestTrue(TEXT("The approved Pizza Memory fixed world request is valid"), FixedWorldYawRequest.IsDataValid());
	TestEqual(TEXT("Fixed world yaw defaults to the approved zero-degree direction"), FixedWorldYawRequest.WorldYawDegrees, 0.0f);

	FRSBossFacingRequest InvalidToleranceRequest = FixedWorldYawRequest;
	InvalidToleranceRequest.YawToleranceDegrees = -KINDA_SMALL_NUMBER;
	TestFalse(TEXT("Negative yaw tolerance is rejected"), InvalidToleranceRequest.IsDataValid());

	return true;
}

#endif
