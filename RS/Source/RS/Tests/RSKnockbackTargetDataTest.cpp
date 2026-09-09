#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

#include "Combat/RSCombatFunctionLibrary.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSKnockbackTargetDataTest, "RS.Combat.KnockbackTargetData", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSKnockbackTargetDataTest::RunTest(const FString& Parameters)
{
	FRSKnockbackTargetData DefaultData;
	FVector NormalizedDirection = FVector::ForwardVector;
	TestFalse(TEXT("Default data has no specified direction"), DefaultData.TryGetNormalizedDirection(NormalizedDirection));
	TestTrue(TEXT("Missing direction resets output"), NormalizedDirection.IsNearlyZero());

	FRSKnockbackTargetData DirectionData;
	DirectionData.bHasKnockbackDirection = true;
	DirectionData.KnockbackDirection = FVector(3.0f, 4.0f, 12.0f);
	DirectionData.Distance = 700.0f;
	DirectionData.Height = 160.0f;
	DirectionData.Duration = 1.25f;

	TestTrue(TEXT("Specified horizontal direction is valid"), DirectionData.TryGetNormalizedDirection(NormalizedDirection));
	TestTrue(TEXT("Specified direction removes Z and normalizes"), NormalizedDirection.Equals(FVector(0.6f, 0.8f, 0.0f)));

	DirectionData.KnockbackDirection = FVector::UpVector;
	TestFalse(TEXT("Vertical-only direction is invalid"), DirectionData.TryGetNormalizedDirection(NormalizedDirection));
	TestTrue(TEXT("Invalid direction remains zero"), NormalizedDirection.IsNearlyZero());

	DirectionData.KnockbackDirection = FVector(3.0f, 4.0f, 12.0f);
	TArray<uint8> SerializedData;
	FMemoryWriter Writer(SerializedData);
	bool bSaveSucceeded = false;
	DirectionData.NetSerialize(Writer, nullptr, bSaveSucceeded);
	TestTrue(TEXT("Direction data serialization succeeds"), bSaveSucceeded);

	FRSKnockbackTargetData LoadedData;
	FMemoryReader Reader(SerializedData);
	bool bLoadSucceeded = false;
	LoadedData.NetSerialize(Reader, nullptr, bLoadSucceeded);
	TestTrue(TEXT("Direction data deserialization succeeds"), bLoadSucceeded);
	TestTrue(TEXT("Direction presence is serialized"), LoadedData.bHasKnockbackDirection);
	TestTrue(TEXT("Direction vector is serialized"), LoadedData.KnockbackDirection.Equals(DirectionData.KnockbackDirection));
	TestEqual(TEXT("Distance is serialized"), LoadedData.Distance, DirectionData.Distance);
	TestEqual(TEXT("Height is serialized"), LoadedData.Height, DirectionData.Height);
	TestEqual(TEXT("Duration is serialized"), LoadedData.Duration, DirectionData.Duration);

	FRSKnockbackTargetData LegacyData;
	LegacyData.Distance = 400.0f;
	LegacyData.Height = 80.0f;
	LegacyData.Duration = 0.5f;
	SerializedData.Reset();
	FMemoryWriter LegacyWriter(SerializedData);
	bool bLegacySaveSucceeded = false;
	LegacyData.NetSerialize(LegacyWriter, nullptr, bLegacySaveSucceeded);
	TestTrue(TEXT("Legacy fallback data serialization succeeds"), bLegacySaveSucceeded);

	LoadedData.bHasKnockbackDirection = true;
	LoadedData.KnockbackDirection = FVector::RightVector;
	FMemoryReader LegacyReader(SerializedData);
	bool bLegacyLoadSucceeded = false;
	LoadedData.NetSerialize(LegacyReader, nullptr, bLegacyLoadSucceeded);
	TestTrue(TEXT("Legacy fallback data deserialization succeeds"), bLegacyLoadSucceeded);
	TestFalse(TEXT("Missing direction remains explicit after serialization"), LoadedData.bHasKnockbackDirection);
	TestTrue(TEXT("Missing direction clears stale vector data"), LoadedData.KnockbackDirection.IsNearlyZero());

	return true;
}

#endif
