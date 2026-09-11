#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include <limits>

#include "Combat/RSArmSwingMath.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSArmSwingMathTest, "RS.Combat.ArmSwing.Math", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSArmSwingMathTest::RunTest(const FString& Parameters)
{
	FRSArmSwingBoxDefinition BoxDefinition;
	BoxDefinition.InnerOffset = 100.0f;
	BoxDefinition.BoxLength = 400.0f;
	BoxDefinition.BoxHalfWidth = 50.0f;
	BoxDefinition.BoxHalfHeight = 60.0f;
	BoxDefinition.BoxCenterHeight = 200.0f;

	FRSArmSwingPathDefinition LeftPath;
	LeftPath.StartYawOffset = -90.0f;
	LeftPath.SweepAngleDegrees = 180.0f;

	TestTrue(TEXT("Valid box definition"), BoxDefinition.IsDataValid());
	TestTrue(TEXT("Valid path definition"), LeftPath.IsDataValid());

	FRSArmSwingBoxDefinition InvalidBoxDefinition = BoxDefinition;
	InvalidBoxDefinition.BoxLength = 0.0f;
	TestFalse(TEXT("Zero box length is invalid"), InvalidBoxDefinition.IsDataValid());

	FRSArmSwingPathDefinition InvalidPath = LeftPath;
	InvalidPath.SweepAngleDegrees = 0.0f;
	TestFalse(TEXT("Zero sweep angle is invalid"), InvalidPath.IsDataValid());

	FTransform LockedAttackTransformFromCapsule;
	const FTransform CapsuleTransform(FRotator::ZeroRotator, FVector(10.0f, 20.0f, 130.0f));
	TestTrue(TEXT("Locked attack transform is calculated from capsule bottom"), FRSArmSwingMath::TryCalculateLockedAttackTransform(CapsuleTransform, 100.0f, FVector(0.0f, 1.0f, 0.0f), LockedAttackTransformFromCapsule));
	TestTrue(TEXT("Locked attack origin uses capsule bottom"), LockedAttackTransformFromCapsule.GetLocation().Equals(FVector(10.0f, 20.0f, 30.0f), 0.01f));
	TestTrue(TEXT("Locked attack yaw uses horizontal actor forward"), LockedAttackTransformFromCapsule.GetRotation().GetForwardVector().Equals(FVector(0.0f, 1.0f, 0.0f), 0.01f));
	TestFalse(TEXT("Vertical actor forward cannot lock attack yaw"), FRSArmSwingMath::TryCalculateLockedAttackTransform(CapsuleTransform, 100.0f, FVector::UpVector, LockedAttackTransformFromCapsule));

	const FTransform LockedAttackTransform(FRotator(0.0f, 90.0f, 0.0f), FVector(10.0f, 20.0f, 30.0f));
	FRSArmSwingBoxSample StartSample;
	TestTrue(TEXT("Start sample is calculated"), FRSArmSwingMath::TryCalculateBoxSample(LockedAttackTransform, BoxDefinition, LeftPath, 0.0f, StartSample));
	TestTrue(TEXT("Start center follows locked yaw and offset"), StartSample.BoxTransform.GetLocation().Equals(FVector(310.0f, 20.0f, 230.0f), 0.01f));
	TestTrue(TEXT("Start box extent uses full length and half sizes"), StartSample.BoxExtent.Equals(FVector(200.0f, 50.0f, 60.0f), 0.01f));
	TestTrue(TEXT("Positive sweep tangent points counterclockwise"), StartSample.TangentDirection.Equals(FVector(0.0f, 1.0f, 0.0f), 0.01f));

	FRSArmSwingBoxSample MiddleSample;
	TestTrue(TEXT("Middle sample is calculated"), FRSArmSwingMath::TryCalculateBoxSample(LockedAttackTransform, BoxDefinition, LeftPath, 0.5f, MiddleSample));
	TestTrue(TEXT("Middle center follows the arc"), MiddleSample.BoxTransform.GetLocation().Equals(FVector(10.0f, 320.0f, 230.0f), 0.01f));
	TestTrue(TEXT("Middle tangent follows the arc"), MiddleSample.TangentDirection.Equals(FVector(-1.0f, 0.0f, 0.0f), 0.01f));

	FRSArmSwingPathDefinition RightPath;
	RightPath.StartYawOffset = -LeftPath.StartYawOffset;
	RightPath.SweepAngleDegrees = -LeftPath.SweepAngleDegrees;
	FRSArmSwingBoxSample RightStartSample;
	TestTrue(TEXT("Mirrored path sample is calculated"), FRSArmSwingMath::TryCalculateBoxSample(LockedAttackTransform, BoxDefinition, RightPath, 0.0f, RightStartSample));
	TestTrue(TEXT("Negative sweep tangent follows the mirrored direction"), RightStartSample.TangentDirection.Equals(FVector(0.0f, 1.0f, 0.0f), 0.01f));

	FRSArmSwingSubstepPlan SmallStepPlan;
	TestTrue(TEXT("Small substep plan is calculated"), FRSArmSwingMath::TryCalculateSubstepPlan(BoxDefinition, LeftPath, 0.0f, 0.01f, SmallStepPlan));
	TestEqual(TEXT("Small movement uses one query"), SmallStepPlan.StepCount, 1);
	TestFalse(TEXT("Small movement does not reach the limit"), SmallStepPlan.bReachedLimit);

	FRSArmSwingSubstepPlan MediumStepPlan;
	TestTrue(TEXT("Medium substep plan is calculated"), FRSArmSwingMath::TryCalculateSubstepPlan(BoxDefinition, LeftPath, 0.0f, 0.1f, MediumStepPlan));
	TestTrue(TEXT("Faster movement adds substeps"), MediumStepPlan.StepCount > SmallStepPlan.StepCount);
	TestFalse(TEXT("Ordinary movement stays below the limit"), MediumStepPlan.bReachedLimit);

	FRSArmSwingSubstepPlan MirroredStepPlan;
	TestTrue(TEXT("Mirrored substep plan is calculated"), FRSArmSwingMath::TryCalculateSubstepPlan(BoxDefinition, RightPath, 0.0f, 0.1f, MirroredStepPlan));
	TestEqual(TEXT("Sweep sign does not change substep count"), MirroredStepPlan.StepCount, MediumStepPlan.StepCount);

	FRSArmSwingSubstepPlan LimitedStepPlan;
	TestTrue(TEXT("Large substep plan is calculated"), FRSArmSwingMath::TryCalculateSubstepPlan(BoxDefinition, LeftPath, 0.0f, 1.0f, LimitedStepPlan));
	TestEqual(TEXT("Large movement is capped"), LimitedStepPlan.StepCount, 16);
	TestTrue(TEXT("Large movement reports the cap"), LimitedStepPlan.bReachedLimit);
	TestTrue(TEXT("Required count preserves the uncapped diagnostic"), LimitedStepPlan.RequiredStepCount > LimitedStepPlan.StepCount);

	FRSArmSwingTelegraphBounds LeftBounds;
	FRSArmSwingTelegraphBounds RightBounds;
	TestTrue(TEXT("Left telegraph bounds are calculated"), FRSArmSwingMath::TryCalculateTelegraphBounds(BoxDefinition, LeftPath, LeftBounds));
	TestTrue(TEXT("Right telegraph bounds are calculated"), FRSArmSwingMath::TryCalculateTelegraphBounds(BoxDefinition, RightPath, RightBounds));
	TestTrue(TEXT("Telegraph inner radius preserves the empty center"), FMath::IsNearlyEqual(LeftBounds.InnerRadius, 100.0f));
	TestTrue(TEXT("Telegraph outer radius includes the outer corner"), FMath::IsNearlyEqual(LeftBounds.OuterRadius, FMath::Sqrt(FMath::Square(500.0f) + FMath::Square(50.0f))));
	TestTrue(TEXT("Mirrored telegraph start is symmetric"), FMath::IsNearlyEqual(RightBounds.StartYawOffset, -LeftBounds.StartYawOffset));
	TestTrue(TEXT("Mirrored telegraph sweep is symmetric"), FMath::IsNearlyEqual(RightBounds.SweepAngleDegrees, -LeftBounds.SweepAngleDegrees));

	FRSArmSwingBoxDefinition PivotTouchingBox = BoxDefinition;
	PivotTouchingBox.InnerOffset = 0.0f;
	FRSArmSwingPathDefinition WidePath = LeftPath;
	WidePath.SweepAngleDegrees = 200.0f;
	FRSArmSwingTelegraphBounds FullCircleBounds;
	TestTrue(TEXT("Pivot touching box telegraph bounds are calculated"), FRSArmSwingMath::TryCalculateTelegraphBounds(PivotTouchingBox, WidePath, FullCircleBounds));
	TestTrue(TEXT("Padded telegraph sweep is capped at a full circle"), FMath::IsNearlyEqual(FullCircleBounds.SweepAngleDegrees, 360.0f));

	const TArray<float> LinearProgress = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };
	TestTrue(TEXT("Linear progress samples are valid"), FRSArmSwingMath::IsProgressSampleSequenceValid(LinearProgress));

	const TArray<float> EasedProgress = { 0.0f, 0.05f, 0.1f, 0.6f, 0.95f, 1.0f };
	TestTrue(TEXT("Accelerating progress samples are valid"), FRSArmSwingMath::IsProgressSampleSequenceValid(EasedProgress));

	const TArray<float> HeldProgress = { 0.0f, 0.0f, 0.0f, 0.5f, 1.0f, 1.0f };
	TestTrue(TEXT("Progress samples may hold still"), FRSArmSwingMath::IsProgressSampleSequenceValid(HeldProgress));

	FString ProgressValidationError;
	const TArray<float> OffsetStartProgress = { 0.2f, 0.5f, 1.0f };
	TestFalse(TEXT("Progress must start at zero"), FRSArmSwingMath::IsProgressSampleSequenceValid(OffsetStartProgress, &ProgressValidationError));
	TestFalse(TEXT("Offset start reports an error"), ProgressValidationError.IsEmpty());

	const TArray<float> ShortEndProgress = { 0.0f, 0.5f, 0.8f };
	TestFalse(TEXT("Progress must end at one"), FRSArmSwingMath::IsProgressSampleSequenceValid(ShortEndProgress));

	const TArray<float> RewindingProgress = { 0.0f, 0.6f, 0.3f, 1.0f };
	TestFalse(TEXT("Progress must not rewind"), FRSArmSwingMath::IsProgressSampleSequenceValid(RewindingProgress));

	const TArray<float> MissingCurveProgress = { 0.0f, 0.0f, 0.0f };
	TestFalse(TEXT("A curve that never advances is invalid"), FRSArmSwingMath::IsProgressSampleSequenceValid(MissingCurveProgress));

	const TArray<float> SingleProgress = { 0.0f };
	TestFalse(TEXT("A single progress sample is invalid"), FRSArmSwingMath::IsProgressSampleSequenceValid(SingleProgress));

	const TArray<float> NonFiniteProgress = { 0.0f, std::numeric_limits<float>::infinity(), 1.0f };
	TestFalse(TEXT("Non-finite progress samples are invalid"), FRSArmSwingMath::IsProgressSampleSequenceValid(NonFiniteProgress));

	return true;
}

#endif
