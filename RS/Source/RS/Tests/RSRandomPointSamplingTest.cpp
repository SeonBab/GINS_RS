#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Abilities/RSGameplayAbility_RandomFallingRocks.h"
#include "Combat/RSRandomPointSampling.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSFallingRocksDefinitionTest, "RS.Combat.FallingRocks.Definition", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSFallingRocksDefinitionTest::RunTest(const FString& Parameters)
{
	FRSRandomFallingRocksDefinition Definition;
	Definition.AttackShape.Type = ERSCombatShapeType::Sphere;
	Definition.AttackShape.Radius = 100.0f;
	Definition.AttackShape.InnerRadius = 0.0f;
	TestTrue(TEXT("Valid falling rock definition"), Definition.IsDataValid());

	FRSRandomFallingRocksDefinition InvalidDefinition = Definition;
	InvalidDefinition.MaxSpawnRadius = InvalidDefinition.MinSpawnRadius;
	TestFalse(TEXT("Equal spawn radii are invalid"), InvalidDefinition.IsDataValid());

	InvalidDefinition = Definition;
	InvalidDefinition.RockInterval = 0.0f;
	TestFalse(TEXT("Zero interval is invalid"), InvalidDefinition.IsDataValid());

	InvalidDefinition = Definition;
	InvalidDefinition.FallEffectLeadTime = InvalidDefinition.ImpactDelay + 0.1f;
	TestFalse(TEXT("Fall effect lead cannot exceed impact delay"), InvalidDefinition.IsDataValid());

	InvalidDefinition = Definition;
	InvalidDefinition.AttackShape.Type = ERSCombatShapeType::Cone;
	TestFalse(TEXT("Non-sphere attack shape is invalid"), InvalidDefinition.IsDataValid());

	InvalidDefinition = Definition;
	InvalidDefinition.AttackShape.InnerRadius = 1.0f;
	TestFalse(TEXT("Attack sphere cannot have an inner radius"), InvalidDefinition.IsDataValid());

	InvalidDefinition = Definition;
	InvalidDefinition.Damage = 10.5f;
	TestFalse(TEXT("Fractional damage is invalid"), InvalidDefinition.IsDataValid());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSRandomPointSamplingAnnulusTest, "RS.Combat.RandomPointSampling.Annulus", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSRandomPointSamplingAnnulusTest::RunTest(const FString& Parameters)
{
	constexpr float MinRadius = 200.0f;
	constexpr float MaxRadius = 1000.0f;
	constexpr int32 SampleCount = 10000;
	FRandomStream RandomStream(12345);

	double NormalizedRadiusSquaredSum = 0.0;
	for (int32 SampleIndex = 0; SampleIndex < SampleCount; ++SampleIndex)
	{
		FVector2D Offset;
		TestTrue(TEXT("Valid annulus generates an offset"), RSRandomPointSampling::TrySamplePointInAnnulus(MinRadius, MaxRadius, RandomStream, Offset));

		const float DistanceSquared = Offset.SquaredLength();
		TestTrue(TEXT("Sample stays inside annulus"), DistanceSquared >= FMath::Square(MinRadius) && DistanceSquared <= FMath::Square(MaxRadius));

		NormalizedRadiusSquaredSum += (DistanceSquared - FMath::Square(MinRadius)) / (FMath::Square(MaxRadius) - FMath::Square(MinRadius));
	}

	const double NormalizedRadiusSquaredMean = NormalizedRadiusSquaredSum / SampleCount;
	TestTrue(TEXT("Radius squared distribution is area-uniform"), FMath::IsNearlyEqual(NormalizedRadiusSquaredMean, 0.5, 0.02));

	FVector2D FirstOffset;
	FVector2D RepeatedFirstOffset;
	FRandomStream FirstRandomStream(777);
	FRandomStream RepeatedFirstRandomStream(777);
	TestTrue(TEXT("First deterministic sample succeeds"), RSRandomPointSampling::TrySamplePointInAnnulus(MinRadius, MaxRadius, FirstRandomStream, FirstOffset));
	TestTrue(TEXT("Repeated deterministic sample succeeds"), RSRandomPointSampling::TrySamplePointInAnnulus(MinRadius, MaxRadius, RepeatedFirstRandomStream, RepeatedFirstOffset));
	TestTrue(TEXT("Equal seeds reproduce the same offset"), FirstOffset.Equals(RepeatedFirstOffset));

	FVector2D InvalidOffset(1.0f, 1.0f);
	FRandomStream InvalidRandomStream(12345);
	TestFalse(TEXT("Invalid annulus fails"), RSRandomPointSampling::TrySamplePointInAnnulus(MaxRadius, MinRadius, InvalidRandomStream, InvalidOffset));
	TestTrue(TEXT("Failed sampling clears output"), InvalidOffset.IsZero());

	return true;
}

#endif
