#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AbilitySystemComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Combat/RSCombatFunctionLibrary.h"
#include "RSGameplayTags.h"
#include "RSCombatShapeTestTypes.h"

namespace RSCombatShapeTest
{
	constexpr ECollisionChannel TargetChannel = ECC_GameTraceChannel2;

	bool ContainsTarget(const AActor& Attacker, const FRSCombatShape& Shape, const FTransform& ShapeTransform, AActor& Target)
	{
		TArray<AActor*> Targets;
		URSCombatFunctionLibrary::FindTargetsInShape(&Attacker, TargetChannel, Shape, ShapeTransform, Targets);

		return Targets.Contains(&Target);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSCombatShapeTest, "RS.Combat.Shape", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSCombatShapeTest::RunTest(const FString& Parameters)
{
	FRSCombatShape BoxValidationShape;
	TestTrue(TEXT("Default box data is valid"), BoxValidationShape.IsDataValid());
	BoxValidationShape.BoxExtent.Z = 0.0f;
	TestFalse(TEXT("Box rejects a zero extent"), BoxValidationShape.IsDataValid());

	FRSCombatShape SphereValidationShape;
	SphereValidationShape.Type = ERSCombatShapeType::Sphere;
	TestTrue(TEXT("Default sphere data is valid"), SphereValidationShape.IsDataValid());
	SphereValidationShape.Radius = 0.0f;
	TestFalse(TEXT("Sphere rejects a zero radius"), SphereValidationShape.IsDataValid());
	SphereValidationShape.Radius = 100.0f;
	SphereValidationShape.InnerRadius = 100.0f;
	TestFalse(TEXT("Sphere rejects an inner radius at its outer boundary"), SphereValidationShape.IsDataValid());

	FRSCombatShape Cone;
	Cone.Type = ERSCombatShapeType::Cone;
	Cone.Range = 100.0f;
	Cone.Angle = 90.0f;

	FString ValidationError;
	TestTrue(TEXT("Valid cone data"), Cone.IsDataValid(&ValidationError));
	TestTrue(TEXT("Valid cone has no validation error"), ValidationError.IsEmpty());

	Cone.Range = 0.0f;
	TestFalse(TEXT("Cone rejects zero range"), Cone.IsDataValid());
	Cone.Range = 100.0f;
	Cone.Angle = 0.0f;
	TestFalse(TEXT("Cone rejects zero angle"), Cone.IsDataValid());
	Cone.Angle = 180.0f;
	TestTrue(TEXT("Cone accepts 180 degrees"), Cone.IsDataValid());
	Cone.Angle = 180.01f;
	TestFalse(TEXT("Cone rejects angles over 180 degrees"), Cone.IsDataValid());
	Cone.Angle = 90.0f;

	const FTransform ConeTransform(FRotator::ZeroRotator, FVector::ZeroVector);
	TestTrue(TEXT("Cone includes its origin in XY"), URSCombatFunctionLibrary::IsLocationInsideCone(Cone, ConeTransform, FVector(0.0f, 0.0f, 500.0f)));
	TestTrue(TEXT("Cone includes its range boundary"), URSCombatFunctionLibrary::IsLocationInsideCone(Cone, ConeTransform, FVector(100.0f, 0.0f, 0.0f)));
	TestFalse(TEXT("Cone excludes beyond its range"), URSCombatFunctionLibrary::IsLocationInsideCone(Cone, ConeTransform, FVector(100.1f, 0.0f, 0.0f)));

	const float BoundaryCoordinate = 50.0f * FMath::InvSqrt(2.0f);
	TestTrue(TEXT("Cone includes its angle boundary"), URSCombatFunctionLibrary::IsLocationInsideCone(Cone, ConeTransform, FVector(BoundaryCoordinate, BoundaryCoordinate, 0.0f)));
	TestFalse(TEXT("Cone excludes outside its angle"), URSCombatFunctionLibrary::IsLocationInsideCone(Cone, ConeTransform, FVector(50.0f, 51.0f, 0.0f)));
	TestFalse(TEXT("Cone excludes points behind it"), URSCombatFunctionLibrary::IsLocationInsideCone(Cone, ConeTransform, FVector(-10.0f, 0.0f, 0.0f)));

	const FTransform RotatedConeTransform(FRotator(0.0f, 90.0f, 0.0f), FVector::ZeroVector);
	TestTrue(TEXT("Cone follows transform yaw"), URSCombatFunctionLibrary::IsLocationInsideCone(Cone, RotatedConeTransform, FVector(0.0f, 50.0f, 0.0f)));
	const FTransform VerticalForwardTransform(FRotator(90.0f, 0.0f, 0.0f), FVector::ZeroVector);
	TestFalse(TEXT("Cone rejects a transform without horizontal forward"), URSCombatFunctionLibrary::IsLocationInsideCone(Cone, VerticalForwardTransform, FVector(10.0f, 0.0f, 0.0f)));

	const FName WorldName = MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("RSCombatShapeTestWorld"), EUniqueObjectNameOptions::GloballyUnique);
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false, WorldName, GetTransientPackage());
	TestWorld->AddToRoot();
	WorldContext.SetCurrentWorld(TestWorld);
	TestWorld->InitializeActorsForPlay(FURL());
	AActor* Attacker = TestWorld->SpawnActor<AActor>();
	ARSCombatShapeTestActor* Target = TestWorld->SpawnActor<ARSCombatShapeTestActor>();

	Target->SetActorLocation(FVector(100.0f, 0.0f, 0.0f));
	TestTrue(TEXT("FindTargetsInShape includes cone range boundary"), RSCombatShapeTest::ContainsTarget(*Attacker, Cone, ConeTransform, *Target));
	Target->GetAbilitySystemComponent()->AddLooseGameplayTag(RSGameplayTags::State_Dead);
	TestFalse(TEXT("FindTargetsInShape keeps excluding dead targets"), RSCombatShapeTest::ContainsTarget(*Attacker, Cone, ConeTransform, *Target));
	Target->GetAbilitySystemComponent()->RemoveLooseGameplayTag(RSGameplayTags::State_Dead);
	Target->SetActorLocation(FVector(-10.0f, 0.0f, 0.0f));
	TestFalse(TEXT("FindTargetsInShape applies cone angle filter"), RSCombatShapeTest::ContainsTarget(*Attacker, Cone, ConeTransform, *Target));

	FRSCombatShape Sphere;
	Sphere.Type = ERSCombatShapeType::Sphere;
	Sphere.Radius = 100.0f;
	Sphere.InnerRadius = 0.0f;
	Target->SetActorLocation(FVector(100.0f, 0.0f, 0.0f));
	TestFalse(TEXT("Sphere keeps excluding its outer boundary"), RSCombatShapeTest::ContainsTarget(*Attacker, Sphere, FTransform::Identity, *Target));
	Target->SetActorLocation(FVector(99.0f, 0.0f, 0.0f));
	TestTrue(TEXT("Sphere keeps including points inside its radius"), RSCombatShapeTest::ContainsTarget(*Attacker, Sphere, FTransform::Identity, *Target));

	Sphere.InnerRadius = 50.0f;
	Target->SetActorLocation(FVector(49.0f, 0.0f, 0.0f));
	TestFalse(TEXT("Donut keeps excluding its inner hole"), RSCombatShapeTest::ContainsTarget(*Attacker, Sphere, FTransform::Identity, *Target));
	Target->SetActorLocation(FVector(50.0f, 0.0f, 0.0f));
	TestTrue(TEXT("Donut keeps including its inner boundary"), RSCombatShapeTest::ContainsTarget(*Attacker, Sphere, FTransform::Identity, *Target));

	FRSCombatShape Box;
	Box.Type = ERSCombatShapeType::Box;
	Box.BoxExtent = FVector(100.0f, 20.0f, 20.0f);
	const FTransform BoxTransform(FRotator(0.0f, 90.0f, 0.0f), FVector::ZeroVector);
	Target->SetActorLocation(FVector(0.0f, 90.0f, 0.0f));
	TestTrue(TEXT("Box keeps following transform rotation"), RSCombatShapeTest::ContainsTarget(*Attacker, Box, BoxTransform, *Target));
	Target->SetActorLocation(FVector(90.0f, 0.0f, 0.0f));
	TestFalse(TEXT("Box keeps excluding outside its rotated extent"), RSCombatShapeTest::ContainsTarget(*Attacker, Box, BoxTransform, *Target));

	TestWorld->RemoveFromRoot();
	GEngine->DestroyWorldContext(TestWorld);
	TestWorld->DestroyWorld(false);

	return true;
}

#endif
