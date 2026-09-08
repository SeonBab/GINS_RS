#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Tasks/RSAbilityTask_ObserveFacing.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSAbilityTaskObserveFacingTest, "RS.Ability.Tasks.ObserveFacing", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSAbilityTaskObserveFacingTest::RunTest(const FString& Parameters)
{
	const FName WorldName = MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("RSObserveFacingTestWorld"), EUniqueObjectNameOptions::GloballyUnique);
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false, WorldName, GetTransientPackage());
	TestWorld->AddToRoot();
	WorldContext.SetCurrentWorld(TestWorld);
	TestWorld->InitializeActorsForPlay(FURL());

	AActor* AvatarActor = TestWorld->SpawnActor<AActor>();
	AvatarActor->SetActorLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);

	bool bHasFacingDirection = false;
	bool bIsWithinYawTolerance = false;
	float YawErrorDegrees = 0.0f;

	URSAbilityTask_ObserveFacing::CalculateFacingState(*AvatarActor, FVector(100.0f, 0.0f, 300.0f), 3.0f, bHasFacingDirection, bIsWithinYawTolerance, YawErrorDegrees);
	TestTrue(TEXT("Forward target has a horizontal direction"), bHasFacingDirection);
	TestTrue(TEXT("Forward target is within tolerance"), bIsWithinYawTolerance);
	TestEqual(TEXT("Forward target has zero yaw error"), YawErrorDegrees, 0.0f);

	URSAbilityTask_ObserveFacing::CalculateFacingState(*AvatarActor, FVector(0.0f, 100.0f, 0.0f), 3.0f, bHasFacingDirection, bIsWithinYawTolerance, YawErrorDegrees);
	TestTrue(TEXT("Left target uses positive shortest yaw"), FMath::IsNearlyEqual(YawErrorDegrees, 90.0f));
	TestFalse(TEXT("Left target is outside tolerance"), bIsWithinYawTolerance);

	URSAbilityTask_ObserveFacing::CalculateFacingState(*AvatarActor, FVector(0.0f, -100.0f, 0.0f), 3.0f, bHasFacingDirection, bIsWithinYawTolerance, YawErrorDegrees);
	TestTrue(TEXT("Right target uses negative shortest yaw"), FMath::IsNearlyEqual(YawErrorDegrees, -90.0f));

	const FVector BoundaryDirection = FRotator(0.0f, 3.0f, 0.0f).Vector() * 100.0f;
	URSAbilityTask_ObserveFacing::CalculateFacingState(*AvatarActor, BoundaryDirection, 3.0f, bHasFacingDirection, bIsWithinYawTolerance, YawErrorDegrees);
	TestTrue(TEXT("Yaw tolerance includes its boundary"), bIsWithinYawTolerance);

	URSAbilityTask_ObserveFacing::CalculateFacingState(*AvatarActor, FVector(0.0f, 0.0f, 500.0f), 3.0f, bHasFacingDirection, bIsWithinYawTolerance, YawErrorDegrees);
	TestFalse(TEXT("Same XY location has no facing direction"), bHasFacingDirection);
	TestFalse(TEXT("Missing direction is not reported as within tolerance"), bIsWithinYawTolerance);

	TestWorld->RemoveFromRoot();
	GEngine->DestroyWorldContext(TestWorld);
	TestWorld->DestroyWorld(false);

	return true;
}

#endif
