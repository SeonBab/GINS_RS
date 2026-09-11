#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Abilities/Boss/RSGameplayAbility_PizzaPattern.h"
#include "Combat/RSPizzaPatternMath.h"

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSPizzaPatternMathTest, "RS.Combat.PizzaPattern.Math", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSPizzaPatternMathTest::RunTest(const FString& Parameters)
{
	FTransform LockedPatternTransform;
	const FTransform CapsuleTransform(FRotator::ZeroRotator, FVector(100.0f, 200.0f, 300.0f));
	TestTrue(TEXT("Locked transform uses capsule bottom and horizontal forward"), RSPizzaPatternMath::TryCalculateLockedPatternTransform(CapsuleTransform, 100.0f, FVector::YAxisVector, LockedPatternTransform));
	TestTrue(TEXT("Locked transform location is capsule bottom"), LockedPatternTransform.GetLocation().Equals(FVector(100.0f, 200.0f, 200.0f)));
	TestTrue(TEXT("Locked transform forward matches actor forward"), LockedPatternTransform.GetUnitAxis(EAxis::X).Equals(FVector::YAxisVector));

	TestEqual(TEXT("Four slices create forty-five degree virtual slices"), RSPizzaPatternMath::CalculateSliceAngleDegrees(4), 45.0f);
	TestEqual(TEXT("Three slices create sixty degree virtual slices"), RSPizzaPatternMath::CalculateSliceAngleDegrees(3), 60.0f);
	TestEqual(TEXT("One slice is rejected"), RSPizzaPatternMath::CalculateSliceAngleDegrees(1), 0.0f);

	const FTransform ForwardLockedTransform(FRotator::ZeroRotator, FVector::ZeroVector);
	TArray<FTransform> SliceTransforms;
	TestTrue(TEXT("A transforms are generated"), RSPizzaPatternMath::TryBuildExplosionSliceTransforms(ForwardLockedTransform, 4, 0, SliceTransforms));
	TestEqual(TEXT("A has configured slice count"), SliceTransforms.Num(), 4);
	const TArray<float> ExpectedAYaws = { 0.0f, 90.0f, 180.0f, 270.0f };
	for (int32 SliceIndex = 0; SliceIndex < SliceTransforms.Num(); ++SliceIndex)
	{
		const float ActualYaw = SliceTransforms[SliceIndex].Rotator().Yaw;
		TestTrue(FString::Printf(TEXT("A slice %d yaw"), SliceIndex), FMath::IsNearlyZero(FMath::FindDeltaAngleDegrees(ActualYaw, ExpectedAYaws[SliceIndex])));
	}

	TestTrue(TEXT("B transforms are generated"), RSPizzaPatternMath::TryBuildExplosionSliceTransforms(ForwardLockedTransform, 4, 1, SliceTransforms));
	const TArray<float> ExpectedBYaws = { 45.0f, 135.0f, 225.0f, 315.0f };
	for (int32 SliceIndex = 0; SliceIndex < SliceTransforms.Num(); ++SliceIndex)
	{
		const float ActualYaw = SliceTransforms[SliceIndex].Rotator().Yaw;
		TestTrue(FString::Printf(TEXT("B slice %d yaw"), SliceIndex), FMath::IsNearlyZero(FMath::FindDeltaAngleDegrees(ActualYaw, ExpectedBYaws[SliceIndex])));
	}

	int64 VirtualSliceIndex = INDEX_NONE;
	TestTrue(TEXT("Forward target is classified"), RSPizzaPatternMath::TryCalculateVirtualSliceIndex(ForwardLockedTransform, 4, FVector(100.0f, 0.0f, 0.0f), VirtualSliceIndex));
	TestEqual(TEXT("Forward is the center of A zero"), VirtualSliceIndex, int64{ 0 });

	const FVector BoundaryTarget = FRotator(0.0f, 22.5f, 0.0f).Vector() * 100.0f;
	TestTrue(TEXT("Boundary target is classified"), RSPizzaPatternMath::TryCalculateVirtualSliceIndex(ForwardLockedTransform, 4, BoundaryTarget, VirtualSliceIndex));
	TestEqual(TEXT("Positive boundary belongs only to the next half-open slice"), VirtualSliceIndex, int64{ 1 });

	const FVector BeforeBoundaryTarget = FRotator(0.0f, 22.49f, 0.0f).Vector() * 100.0f;
	TestTrue(TEXT("Target before boundary is classified"), RSPizzaPatternMath::TryCalculateVirtualSliceIndex(ForwardLockedTransform, 4, BeforeBoundaryTarget, VirtualSliceIndex));
	TestEqual(TEXT("Target before boundary remains in A zero"), VirtualSliceIndex, int64{ 0 });

	TestTrue(TEXT("Center belongs to first A explosion"), RSPizzaPatternMath::IsLocationInExplosionGroup(ForwardLockedTransform, 4, 0, FVector::ZeroVector));
	TestFalse(TEXT("Center does not also belong to B explosion"), RSPizzaPatternMath::IsLocationInExplosionGroup(ForwardLockedTransform, 4, 1, FVector::ZeroVector));
	TestTrue(TEXT("Third explosion returns to A"), RSPizzaPatternMath::IsLocationInExplosionGroup(ForwardLockedTransform, 4, 2, FVector(100.0f, 0.0f, 0.0f)));

	return true;
}

#endif
