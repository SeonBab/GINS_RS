#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Abilities/RSGameplayAbility_SequentialSweepExplosion.h"
#include "Combat/RSSequentialSweepExplosionMath.h"
#include "RSGameplayTags.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSSequentialSweepExplosionDefinitionTest, "RS.Combat.SequentialSweepExplosion.Definition", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSSequentialSweepExplosionDefinitionTest::RunTest(const FString& Parameters)
{
	FRSSequentialSweepExplosionDefinition Definition;
	TestTrue(TEXT("Default definition is valid"), Definition.IsDataValid());

	const URSGameplayAbility_SequentialSweepExplosion* AbilityCDO = GetDefault<URSGameplayAbility_SequentialSweepExplosion>();
	TestTrue(TEXT("Ability has its identifying asset tag"), AbilityCDO && AbilityCDO->GetAssetTags().HasTagExact(RSGameplayTags::Ability_Combat_SequentialSweepExplosion));

	FRSSequentialSweepExplosionDefinition InvalidDefinition = Definition;
	InvalidDefinition.OuterRadius = 0.0f;
	TestFalse(TEXT("Zero outer radius is invalid"), InvalidDefinition.IsDataValid());

	InvalidDefinition = Definition;
	InvalidDefinition.TotalSweepAngleDegrees = 361.0f;
	TestFalse(TEXT("Sweep above full circle is invalid"), InvalidDefinition.IsDataValid());

	InvalidDefinition = Definition;
	InvalidDefinition.SectorCount = 0;
	TestFalse(TEXT("Zero sectors are invalid"), InvalidDefinition.IsDataValid());

	InvalidDefinition = Definition;
	InvalidDefinition.FirstExplosionDelay = 0.0f;
	TestFalse(TEXT("Zero first explosion delay is invalid"), InvalidDefinition.IsDataValid());

	InvalidDefinition = Definition;
	InvalidDefinition.SectorExplosionInterval = 0.0f;
	TestFalse(TEXT("Zero explosion interval is invalid"), InvalidDefinition.IsDataValid());

	InvalidDefinition = Definition;
	InvalidDefinition.Damage = 10.5f;
	TestFalse(TEXT("Fractional damage is invalid"), InvalidDefinition.IsDataValid());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSSequentialSweepExplosionMathTest, "RS.Combat.SequentialSweepExplosion.Math", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSSequentialSweepExplosionMathTest::RunTest(const FString& Parameters)
{
	FTransform LockedTransform;
	const FTransform CapsuleTransform(FRotator::ZeroRotator, FVector(100.0f, 200.0f, 300.0f));
	TestTrue(TEXT("Locked transform is calculated"), RSSequentialSweepExplosionMath::TryCalculateLockedAttackTransform(CapsuleTransform, 100.0f, FVector::YAxisVector, LockedTransform));
	TestTrue(TEXT("Locked transform uses capsule bottom"), LockedTransform.GetLocation().Equals(FVector(100.0f, 200.0f, 200.0f)));
	TestTrue(TEXT("Locked transform uses horizontal actor forward"), LockedTransform.GetUnitAxis(EAxis::X).Equals(FVector::YAxisVector));

	TestEqual(TEXT("One hundred twenty degrees split into four sectors"), RSSequentialSweepExplosionMath::CalculateSectorAngleDegrees(120.0f, 4), 30.0f);
	TestEqual(TEXT("Invalid sector count returns zero angle"), RSSequentialSweepExplosionMath::CalculateSectorAngleDegrees(120.0f, 0), 0.0f);

	float SectorStartAngleDegrees = 0.0f;
	float SectorSweepAngleDegrees = 0.0f;
	TestTrue(TEXT("Third sector angles are calculated"), RSSequentialSweepExplosionMath::TryCalculateSectorAngles(120.0f, 4, -60.0f, 2, SectorStartAngleDegrees, SectorSweepAngleDegrees));
	TestEqual(TEXT("Third sector starts at zero degrees"), SectorStartAngleDegrees, 0.0f);
	TestEqual(TEXT("Third sector sweeps thirty degrees"), SectorSweepAngleDegrees, 30.0f);

	TestEqual(TEXT("Timeline starts with one warning"), RSSequentialSweepExplosionMath::CalculateRequiredWarningCount(0.0f, 1.0f, 4), 1);
	TestEqual(TEXT("Before first warning boundary only one is visible"), RSSequentialSweepExplosionMath::CalculateRequiredWarningCount(0.24f, 1.0f, 4), 1);
	TestEqual(TEXT("Second warning appears at first boundary"), RSSequentialSweepExplosionMath::CalculateRequiredWarningCount(0.25f, 1.0f, 4), 2);
	TestEqual(TEXT("Large step catches up all warnings"), RSSequentialSweepExplosionMath::CalculateRequiredWarningCount(1.5f, 1.0f, 4), 4);

	TestEqual(TEXT("No explosion occurs before first delay"), RSSequentialSweepExplosionMath::CalculateRequiredExplosionCount(0.99f, 1.0f, 0.2f, 4), 0);
	TestEqual(TEXT("First explosion occurs at first delay"), RSSequentialSweepExplosionMath::CalculateRequiredExplosionCount(1.0f, 1.0f, 0.2f, 4), 1);
	TestEqual(TEXT("Second explosion occurs after one interval"), RSSequentialSweepExplosionMath::CalculateRequiredExplosionCount(1.2f, 1.0f, 0.2f, 4), 2);
	TestEqual(TEXT("Large step catches up all explosions"), RSSequentialSweepExplosionMath::CalculateRequiredExplosionCount(2.0f, 1.0f, 0.2f, 4), 4);
	TestTrue(TEXT("Total duration includes final interval"), FMath::IsNearlyEqual(RSSequentialSweepExplosionMath::CalculateTotalDuration(1.0f, 0.2f, 4), 1.6f));

	const FTransform ForwardLockedTransform(FRotator::ZeroRotator, FVector::ZeroVector);
	const FVector SharedBoundaryLocation = FRotator(0.0f, -30.0f, 0.0f).Vector() * 500.0f;
	TestTrue(TEXT("Shared boundary belongs to first sector"), RSSequentialSweepExplosionMath::IsLocationInSector(ForwardLockedTransform, 500.0f, 120.0f, 4, -60.0f, 0, SharedBoundaryLocation));
	TestTrue(TEXT("Shared boundary also belongs to second sector"), RSSequentialSweepExplosionMath::IsLocationInSector(ForwardLockedTransform, 500.0f, 120.0f, 4, -60.0f, 1, SharedBoundaryLocation));

	const FVector FirstSectorMiddleLocation = FRotator(0.0f, -45.0f, 0.0f).Vector() * 250.0f;
	TestTrue(TEXT("First sector contains its middle"), RSSequentialSweepExplosionMath::IsLocationInSector(ForwardLockedTransform, 500.0f, 120.0f, 4, -60.0f, 0, FirstSectorMiddleLocation));
	TestFalse(TEXT("Second sector excludes first sector middle"), RSSequentialSweepExplosionMath::IsLocationInSector(ForwardLockedTransform, 500.0f, 120.0f, 4, -60.0f, 1, FirstSectorMiddleLocation));

	const FVector OuterBoundaryLocation = FRotator(0.0f, -45.0f, 0.0f).Vector() * 500.0f;
	const FVector OutsideRadiusLocation = FRotator(0.0f, -45.0f, 0.0f).Vector() * 500.1f;
	TestTrue(TEXT("Outer radius boundary is included"), RSSequentialSweepExplosionMath::IsLocationInSector(ForwardLockedTransform, 500.0f, 120.0f, 4, -60.0f, 0, OuterBoundaryLocation));
	TestFalse(TEXT("Location outside outer radius is excluded"), RSSequentialSweepExplosionMath::IsLocationInSector(ForwardLockedTransform, 500.0f, 120.0f, 4, -60.0f, 0, OutsideRadiusLocation));
	TestTrue(TEXT("Center belongs to first sector"), RSSequentialSweepExplosionMath::IsLocationInSector(ForwardLockedTransform, 500.0f, 120.0f, 4, -60.0f, 0, FVector::ZeroVector));
	TestTrue(TEXT("Center also belongs to fourth sector"), RSSequentialSweepExplosionMath::IsLocationInSector(ForwardLockedTransform, 500.0f, 120.0f, 4, -60.0f, 3, FVector::ZeroVector));

	return true;
}

#endif
