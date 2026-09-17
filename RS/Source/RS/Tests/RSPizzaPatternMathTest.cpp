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
	Definition.TelegraphCueDuration = 0.6f;
	Definition.TelegraphCueGap = 0.2f;
	Definition.RecallDelay = 1.0f;
	Definition.ExplosionInterval = 0.8f;
	TestTrue(TEXT("Valid pizza pattern definition"), Definition.IsDataValid());

	const FRSPizzaCueTimings CueTimings = Definition.MakeCueTimings();
	TestEqual(TEXT("Cue timings use the telegraph cue duration"), CueTimings.CueDuration, Definition.TelegraphCueDuration);
	TestEqual(TEXT("Cue timings use the telegraph cue gap"), CueTimings.CueGap, Definition.TelegraphCueGap);
	TestEqual(TEXT("Cue timings use the recall delay"), CueTimings.RecallDelay, Definition.RecallDelay);
	TestEqual(TEXT("Cue timings use the explosion interval"), CueTimings.ExplosionInterval, Definition.ExplosionInterval);

	// 예고, 판정과 연출이 같은 조각을 쓰므로 안쪽 경계도 이 한 곳에서 실립니다
	FRSCombatShape SliceShape;
	TestTrue(TEXT("A slice shape is built"), Definition.TryMakeSliceShape(SliceShape));
	TestEqual(TEXT("A slice shape without an inner radius starts at the pattern origin"), SliceShape.InnerRadius, 0.0f);
	TestEqual(TEXT("A slice shape uses the authored outer radius"), SliceShape.OuterRadius, Definition.OuterRadius);
	TestEqual(TEXT("A slice shape spans one slice"), SliceShape.SweepAngleDegrees, Definition.CalculateSliceAngleDegrees());

	// 에셋이 적은 안쪽 경계가 그대로 실려야 에디터에 적은 값과 실제 판정 범위가 같습니다
	FRSPizzaPatternDefinition InnerRadiusDefinition = Definition;
	InnerRadiusDefinition.InnerRadius = 120.0f;
	FRSCombatShape InnerRadiusSliceShape;
	TestTrue(TEXT("A slice shape with an authored inner radius is built"), InnerRadiusDefinition.TryMakeSliceShape(InnerRadiusSliceShape));
	TestEqual(TEXT("A slice shape carries the authored inner radius"), InnerRadiusSliceShape.InnerRadius, 120.0f);
	TestEqual(TEXT("A slice shape with an inner radius keeps its angle"), InnerRadiusSliceShape.SweepAngleDegrees, Definition.CalculateSliceAngleDegrees());

	// 안쪽 경계가 바깥을 삼키면 판정할 면적이 없으므로 편집 시점과 런타임이 모두 그 사실을 알아야 합니다
	FRSPizzaPatternDefinition SwallowedDefinition = Definition;
	SwallowedDefinition.InnerRadius = Definition.OuterRadius;
	FRSCombatShape SwallowedSliceShape;
	TestFalse(TEXT("An inner radius at the outer radius leaves no slice"), SwallowedDefinition.TryMakeSliceShape(SwallowedSliceShape));
	TestFalse(TEXT("An inner radius at the outer radius fails validation"), SwallowedDefinition.IsDataValid());

	FRSPizzaPatternDefinition InvalidSliceDefinition = Definition;
	InvalidSliceDefinition.SliceCount = 1;
	FRSCombatShape InvalidSliceShape;
	TestFalse(TEXT("An invalid definition builds no slice"), InvalidSliceDefinition.TryMakeSliceShape(InvalidSliceShape));

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
	InvalidDefinition.TelegraphCueDuration = 0.0f;
	TestFalse(TEXT("Zero telegraph cue duration is invalid"), InvalidDefinition.IsDataValid());

	InvalidDefinition = Definition;
	InvalidDefinition.RecallDelay = -0.1f;
	TestFalse(TEXT("Negative recall delay is invalid"), InvalidDefinition.IsDataValid());

	InvalidDefinition = Definition;
	InvalidDefinition.TelegraphCueGap = 0.0f;
	TestFalse(TEXT("Zero telegraph cue gap is invalid for two explosions"), InvalidDefinition.IsDataValid());

	InvalidDefinition = Definition;
	InvalidDefinition.ExplosionInterval = 0.0f;
	TestFalse(TEXT("Zero explosion interval is invalid for two explosions"), InvalidDefinition.IsDataValid());

	// 1회형은 예고 사이와 폭발 사이가 없으므로 두 간격을 설정하지 않아도 유효합니다
	FRSPizzaPatternDefinition SingleExplosionDefinition = Definition;
	SingleExplosionDefinition.ExplosionCount = 1;
	SingleExplosionDefinition.TelegraphCueGap = 0.0f;
	SingleExplosionDefinition.ExplosionInterval = 0.0f;
	TestTrue(TEXT("Single explosion definition does not require gap or interval"), SingleExplosionDefinition.IsDataValid());

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
