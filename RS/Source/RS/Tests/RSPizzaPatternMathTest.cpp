#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Abilities/Boss/RSGameplayAbility_PizzaPattern.h"
#include "Combat/RSCircularSliceMath.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSPizzaPatternDefinitionTest, "RS.Combat.PizzaPattern.Definition", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSPizzaPatternDefinitionTest::RunTest(const FString& Parameters)
{
	FRSPizzaPatternDefinition Definition;
	Definition.SliceCount = 4;
	Definition.ExplosionCount = 2;
	Definition.OuterRadius = 1000.0f;
	Definition.AttackStartDelay = 0.0f;
	Definition.TelegraphDuration = 1.0f;
	Definition.Damage = 10.0f;
	TestTrue(TEXT("Valid pizza pattern definition"), Definition.IsDataValid());

	FRSPizzaPatternDefinition InvalidDefinition = Definition;
	InvalidDefinition.SliceCount = 1;
	TestFalse(TEXT("Slice count below two is invalid"), InvalidDefinition.IsDataValid());

	InvalidDefinition = Definition;
	InvalidDefinition.ExplosionCount = 0;
	TestFalse(TEXT("Explosion count below one is invalid"), InvalidDefinition.IsDataValid());

	InvalidDefinition = Definition;
	InvalidDefinition.OuterRadius = 0.0f;
	TestFalse(TEXT("Zero outer radius is invalid"), InvalidDefinition.IsDataValid());

	InvalidDefinition = Definition;
	InvalidDefinition.AttackStartDelay = -0.1f;
	TestFalse(TEXT("Negative attack start delay is invalid"), InvalidDefinition.IsDataValid());

	InvalidDefinition = Definition;
	InvalidDefinition.TelegraphDuration = 0.0f;
	TestFalse(TEXT("Zero telegraph duration is invalid"), InvalidDefinition.IsDataValid());

	InvalidDefinition = Definition;
	InvalidDefinition.Damage = 10.5f;
	TestFalse(TEXT("Fractional damage is invalid"), InvalidDefinition.IsDataValid());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSCircularSliceMathTest, "RS.Combat.CircularSlice.Math", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSCircularSliceMathTest::RunTest(const FString& Parameters)
{
	FTransform LockedPatternTransform;
	const FTransform CapsuleTransform(FRotator::ZeroRotator, FVector(100.0f, 200.0f, 300.0f));
	TestTrue(TEXT("Locked transform uses capsule bottom and horizontal forward"), RSCircularSliceMath::TryCalculateLockedTransformFromCapsule(CapsuleTransform, 100.0f, FVector::YAxisVector, LockedPatternTransform));
	TestTrue(TEXT("Locked transform location is capsule bottom"), LockedPatternTransform.GetLocation().Equals(FVector(100.0f, 200.0f, 200.0f)));
	TestTrue(TEXT("Locked transform forward matches actor forward"), LockedPatternTransform.GetUnitAxis(EAxis::X).Equals(FVector::YAxisVector));

	TestEqual(TEXT("Eight slices create forty-five degree slices"), RSCircularSliceMath::CalculateSliceAngleDegrees(8), 45.0f);
	TestEqual(TEXT("Six slices create sixty degree slices"), RSCircularSliceMath::CalculateSliceAngleDegrees(6), 60.0f);
	TestEqual(TEXT("One slice is rejected"), RSCircularSliceMath::CalculateSliceAngleDegrees(1), 0.0f);

	const FTransform ForwardLockedTransform(FRotator::ZeroRotator, FVector::ZeroVector);
	const TArray<float> ExpectedSliceYaws = { 0.0f, 45.0f, 90.0f, 135.0f, 180.0f, 225.0f, 270.0f, 315.0f };
	for (int32 SliceIndex = 0; SliceIndex < ExpectedSliceYaws.Num(); ++SliceIndex)
	{
		FTransform SliceTransform;
		TestTrue(FString::Printf(TEXT("Slice %d transform is generated"), SliceIndex), RSCircularSliceMath::TryBuildSliceTransform(ForwardLockedTransform, 8, SliceIndex, SliceTransform));
		TestTrue(FString::Printf(TEXT("Slice %d yaw"), SliceIndex), FMath::IsNearlyZero(FMath::FindDeltaAngleDegrees(SliceTransform.Rotator().Yaw, ExpectedSliceYaws[SliceIndex])));
	}

	int32 SliceIndex = INDEX_NONE;
	TestTrue(TEXT("Forward target is classified"), RSCircularSliceMath::TryCalculateSliceIndex(ForwardLockedTransform, 8, FVector(100.0f, 0.0f, 0.0f), SliceIndex));
	TestEqual(TEXT("Forward is the center of slice zero"), SliceIndex, 0);

	const FVector BoundaryTarget = FRotator(0.0f, 22.5f, 0.0f).Vector() * 100.0f;
	TestTrue(TEXT("Boundary target is classified"), RSCircularSliceMath::TryCalculateSliceIndex(ForwardLockedTransform, 8, BoundaryTarget, SliceIndex));
	TestEqual(TEXT("Positive boundary belongs only to the next half-open slice"), SliceIndex, 1);

	const FVector BeforeBoundaryTarget = FRotator(0.0f, 22.49f, 0.0f).Vector() * 100.0f;
	TestTrue(TEXT("Target before boundary is classified"), RSCircularSliceMath::TryCalculateSliceIndex(ForwardLockedTransform, 8, BeforeBoundaryTarget, SliceIndex));
	TestEqual(TEXT("Target before boundary remains in slice zero"), SliceIndex, 0);

	TestTrue(TEXT("Center is classified"), RSCircularSliceMath::TryCalculateSliceIndex(ForwardLockedTransform, 8, FVector::ZeroVector, SliceIndex));
	TestEqual(TEXT("Center belongs to slice zero"), SliceIndex, 0);
	TestTrue(TEXT("Zero and four are opposite"), RSCircularSliceMath::AreSlicesOpposite(8, 0, 4));
	TestTrue(TEXT("One and five are opposite"), RSCircularSliceMath::AreSlicesOpposite(8, 1, 5));
	TestFalse(TEXT("Zero and three are not opposite"), RSCircularSliceMath::AreSlicesOpposite(8, 0, 3));
	TestFalse(TEXT("Odd slice counts have no exact opposite pair"), RSCircularSliceMath::AreSlicesOpposite(7, 0, 3));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSPizzaPatternMathTest, "RS.Combat.PizzaPattern.Math", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSPizzaPatternMathTest::RunTest(const FString& Parameters)
{
	FRSPizzaPatternDefinition Definition;
	Definition.SliceCount = 4;

	TestEqual(TEXT("Four configured slices create eight virtual slices"), Definition.CalculateVirtualSliceCount(), 8);
	TestEqual(TEXT("Eight virtual slices create forty-five degree slices"), Definition.CalculateSliceAngleDegrees(), 45.0f);

	const FTransform ForwardLockedTransform(FRotator::ZeroRotator, FVector::ZeroVector);
	TArray<FTransform> SliceTransforms;
	TestTrue(TEXT("A transforms are generated"), Definition.TryBuildExplosionSliceTransforms(ForwardLockedTransform, 0, SliceTransforms));
	TestEqual(TEXT("A has configured slice count"), SliceTransforms.Num(), 4);
	const TArray<float> ExpectedAYaws = { 0.0f, 90.0f, 180.0f, 270.0f };
	for (int32 SliceIndex = 0; SliceIndex < SliceTransforms.Num(); ++SliceIndex)
	{
		TestTrue(FString::Printf(TEXT("A slice %d yaw"), SliceIndex), FMath::IsNearlyZero(FMath::FindDeltaAngleDegrees(SliceTransforms[SliceIndex].Rotator().Yaw, ExpectedAYaws[SliceIndex])));
	}

	TestTrue(TEXT("B transforms are generated"), Definition.TryBuildExplosionSliceTransforms(ForwardLockedTransform, 1, SliceTransforms));
	const TArray<float> ExpectedBYaws = { 45.0f, 135.0f, 225.0f, 315.0f };
	for (int32 SliceIndex = 0; SliceIndex < SliceTransforms.Num(); ++SliceIndex)
	{
		TestTrue(FString::Printf(TEXT("B slice %d yaw"), SliceIndex), FMath::IsNearlyZero(FMath::FindDeltaAngleDegrees(SliceTransforms[SliceIndex].Rotator().Yaw, ExpectedBYaws[SliceIndex])));
	}

	TestTrue(TEXT("Center belongs to first A explosion"), Definition.IsLocationInExplosionGroup(ForwardLockedTransform, 0, FVector::ZeroVector));
	TestFalse(TEXT("Center does not also belong to B explosion"), Definition.IsLocationInExplosionGroup(ForwardLockedTransform, 1, FVector::ZeroVector));
	TestTrue(TEXT("Third explosion returns to A"), Definition.IsLocationInExplosionGroup(ForwardLockedTransform, 2, FVector(100.0f, 0.0f, 0.0f)));

	return true;
}

#endif
