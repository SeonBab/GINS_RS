#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "RSBaseGameplayAbility_BossPattern.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSBossPatternNiagaraTransformArrayTest, "RS.Boss.PatternNiagara.TransformArray", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSBossPatternNiagaraTransformArrayTest::RunTest(const FString& Parameters)
{
	const FRSBossPatternNiagaraEntry DefaultEntry;
	TestEqual(TEXT("Existing entries keep individual system spawning by default"), DefaultEntry.SpawnPolicy, ERSBossPatternNiagaraSpawnPolicy::IndividualSystems);

	const FTransform SystemTransform(FQuat::Identity, FVector(100.0f, 200.0f, 10.0f), FVector::OneVector);
	const TArray<FTransform> WorldTransforms =
	{
		FTransform(FRotator(0.0f, 30.0f, 0.0f), FVector(150.0f, 250.0f, 10.0f), FVector(2.0f, 3.0f, 4.0f)),
		FTransform(FRotator(0.0f, -45.0f, 0.0f), FVector(75.0f, 300.0f, 30.0f), FVector(0.5f, 0.75f, 1.25f))
	};

	TArray<FVector> LocalPositions;
	TArray<FQuat> LocalRotations;
	TArray<FVector> LocalScales;
	FBox LocalBounds(EForceInit::ForceInit);
	TestTrue(TEXT("Valid world transforms build Niagara arrays"), URSBaseGameplayAbility_BossPattern::BuildNiagaraTransformArraysForTest(WorldTransforms, SystemTransform, 25.0f, LocalPositions, LocalRotations, LocalScales, LocalBounds));
	TestEqual(TEXT("Every transform creates one position"), LocalPositions.Num(), WorldTransforms.Num());
	TestEqual(TEXT("Every transform creates one rotation"), LocalRotations.Num(), WorldTransforms.Num());
	TestEqual(TEXT("Every transform creates one scale"), LocalScales.Num(), WorldTransforms.Num());

	if (LocalPositions.Num() == WorldTransforms.Num() && LocalRotations.Num() == WorldTransforms.Num() && LocalScales.Num() == WorldTransforms.Num())
	{
		TestTrue(TEXT("First position is relative to the system origin"), LocalPositions[0].Equals(FVector(50.0f, 50.0f, 0.0f)));
		TestTrue(TEXT("Second position is relative to the system origin"), LocalPositions[1].Equals(FVector(-25.0f, 100.0f, 20.0f)));
		TestTrue(TEXT("First rotation is preserved"), LocalRotations[0].Equals(WorldTransforms[0].GetRotation()));
		TestTrue(TEXT("Second rotation is preserved"), LocalRotations[1].Equals(WorldTransforms[1].GetRotation()));
		TestTrue(TEXT("First scale is preserved"), LocalScales[0].Equals(FVector(2.0f, 3.0f, 4.0f)));
		TestTrue(TEXT("Second scale is preserved"), LocalScales[1].Equals(FVector(0.5f, 0.75f, 1.25f)));
	}

	TestTrue(TEXT("Bounds minimum includes padding"), LocalBounds.Min.Equals(FVector(-50.0f, 25.0f, -25.0f)));
	TestTrue(TEXT("Bounds maximum includes padding"), LocalBounds.Max.Equals(FVector(75.0f, 125.0f, 45.0f)));

	TArray<FVector> UntouchedPositions = { FVector(1.0f, 2.0f, 3.0f) };
	TArray<FQuat> UntouchedRotations = { FQuat::Identity };
	TArray<FVector> UntouchedScales = { FVector::OneVector };
	FBox UntouchedBounds(FVector::ZeroVector, FVector::OneVector);
	TestFalse(TEXT("Negative bounds padding fails"), URSBaseGameplayAbility_BossPattern::BuildNiagaraTransformArraysForTest(WorldTransforms, SystemTransform, -1.0f, UntouchedPositions, UntouchedRotations, UntouchedScales, UntouchedBounds));
	TestEqual(TEXT("Failed conversion leaves positions untouched"), UntouchedPositions.Num(), 1);
	TestEqual(TEXT("Failed conversion leaves rotations untouched"), UntouchedRotations.Num(), 1);
	TestEqual(TEXT("Failed conversion leaves scales untouched"), UntouchedScales.Num(), 1);
	TestTrue(TEXT("Failed conversion leaves bounds minimum untouched"), UntouchedBounds.Min.Equals(FVector::ZeroVector));
	TestTrue(TEXT("Failed conversion leaves bounds maximum untouched"), UntouchedBounds.Max.Equals(FVector::OneVector));

	return true;
}

#endif
