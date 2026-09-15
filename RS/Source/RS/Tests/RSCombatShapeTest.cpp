#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AbilitySystemComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSConeFillTest, "RS.Combat.ConeFill", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSConeFillTest::RunTest(const FString& Parameters)
{
	FRSCombatShape Cone;
	Cone.Type = ERSCombatShapeType::Cone;
	Cone.Range = 600.0f;
	Cone.Angle = 90.0f;

	constexpr float Spacing = 100.0f;
	const FTransform ConeTransform(FRotator(0.0f, 35.0f, 0.0f), FVector(1200.0f, -400.0f, 250.0f));

	TArray<FTransform> FillTransforms;
	TestTrue(TEXT("Valid cone builds fill transforms"), URSCombatFunctionLibrary::BuildShapeFillTransforms(Cone, ConeTransform, Spacing, FillTransforms));
	TestTrue(TEXT("Valid cone produces at least one transform"), FillTransforms.Num() > 0);

	// 연출이 판정 범위를 넘지 않아야 하므로 모든 칸이 같은 포함 규칙을 통과하는지 확인합니다
	const FVector ConeApex = ConeTransform.GetLocation();
	FVector ConeForward = ConeTransform.GetUnitAxis(EAxis::X);
	ConeForward.Z = 0.0f;
	ConeForward.Normalize();
	const FVector ConeRight = FVector::CrossProduct(FVector::UpVector, ConeForward);

	bool bAllInsideCone = true;
	bool bAllOnGrid = true;
	bool bAllKeepPlane = true;
	bool bAllKeepRotation = true;
	for (const FTransform& FillTransform : FillTransforms)
	{
		const FVector FillLocation = FillTransform.GetLocation();
		if (!URSCombatFunctionLibrary::IsLocationInsideCone(Cone, ConeTransform, FillLocation))
		{
			bAllInsideCone = false;
		}

		const FVector Offset = FillLocation - ConeApex;
		const float ForwardSteps = FVector::DotProduct(Offset, ConeForward) / Spacing;
		const float RightSteps = FVector::DotProduct(Offset, ConeRight) / Spacing;
		if (!FMath::IsNearlyEqual(ForwardSteps, FMath::RoundToFloat(ForwardSteps), 0.01f) || !FMath::IsNearlyEqual(RightSteps, FMath::RoundToFloat(RightSteps), 0.01f))
		{
			bAllOnGrid = false;
		}

		if (!FMath::IsNearlyEqual(FillLocation.Z, ConeApex.Z))
		{
			bAllKeepPlane = false;
		}

		if (!FillTransform.GetRotation().Equals(ConeTransform.GetRotation()))
		{
			bAllKeepRotation = false;
		}
	}

	TestTrue(TEXT("Every fill transform stays inside the cone"), bAllInsideCone);
	TestTrue(TEXT("Every fill transform lands on a spacing grid step"), bAllOnGrid);
	TestTrue(TEXT("Every fill transform keeps the cone plane height"), bAllKeepPlane);
	TestTrue(TEXT("Every fill transform inherits the cone rotation"), bAllKeepRotation);

	// 간격을 절반으로 줄이면 같은 면적을 네 배로 채우므로 밀도가 간격을 따라 변하는지 확인합니다
	TArray<FTransform> DenseFillTransforms;
	TestTrue(TEXT("Halved spacing builds fill transforms"), URSCombatFunctionLibrary::BuildShapeFillTransforms(Cone, ConeTransform, Spacing * 0.5f, DenseFillTransforms));
	TestTrue(TEXT("Halved spacing multiplies the fill count"), DenseFillTransforms.Num() > FillTransforms.Num() * 3);

	// 간격이 범위보다 넓어도 연출이 사라지면 안 되므로 중심선 가운데 한 개로 대신하는지 확인합니다
	TArray<FTransform> SingleFillTransforms;
	TestTrue(TEXT("Spacing wider than the range builds fill transforms"), URSCombatFunctionLibrary::BuildShapeFillTransforms(Cone, ConeTransform, Cone.Range * 2.0f, SingleFillTransforms));
	TestEqual(TEXT("Spacing wider than the range fills exactly one cell"), SingleFillTransforms.Num(), 1);

	if (SingleFillTransforms.Num() == 1)
	{
		const FVector ExpectedCenter = ConeApex + ConeForward * (Cone.Range * 0.5f);
		TestTrue(TEXT("The single fill transform sits at the cone center"), SingleFillTransforms[0].GetLocation().Equals(ExpectedCenter));
		TestTrue(TEXT("The single fill transform stays inside the cone"), URSCombatFunctionLibrary::IsLocationInsideCone(Cone, ConeTransform, SingleFillTransforms[0].GetLocation()));
		TestTrue(TEXT("The single fill transform inherits the cone rotation"), SingleFillTransforms[0].GetRotation().Equals(ConeTransform.GetRotation()));
	}

	// 잘못된 입력에서는 호출자가 이전 결과를 그대로 쓰지 않도록 실패를 알리고 배열을 건드리지 않습니다
	TArray<FTransform> UntouchedTransforms;
	UntouchedTransforms.Add(FTransform::Identity);

	TestFalse(TEXT("Zero spacing fails"), URSCombatFunctionLibrary::BuildShapeFillTransforms(Cone, ConeTransform, 0.0f, UntouchedTransforms));
	TestFalse(TEXT("Negative spacing fails"), URSCombatFunctionLibrary::BuildShapeFillTransforms(Cone, ConeTransform, -50.0f, UntouchedTransforms));

	// Box를 채우는 보스 패턴이 없어 지원하지 않으므로 조용히 비는 대신 실패를 알립니다
	FRSCombatShape BoxShape;
	BoxShape.Type = ERSCombatShapeType::Box;
	TestFalse(TEXT("Box shape fails"), URSCombatFunctionLibrary::BuildShapeFillTransforms(BoxShape, ConeTransform, Spacing, UntouchedTransforms));

	FRSCombatShape InvalidCone;
	InvalidCone.Type = ERSCombatShapeType::Cone;
	InvalidCone.Range = 0.0f;
	InvalidCone.Angle = 90.0f;
	TestFalse(TEXT("Zero range cone fails"), URSCombatFunctionLibrary::BuildShapeFillTransforms(InvalidCone, ConeTransform, Spacing, UntouchedTransforms));

	// 간격이 과도하게 좁으면 순회 자체가 폭주하므로 채우기 전에 상한으로 막습니다
	TestFalse(TEXT("Runaway cell count fails"), URSCombatFunctionLibrary::BuildShapeFillTransforms(Cone, ConeTransform, 1.0f, UntouchedTransforms));

	TestEqual(TEXT("Failed calls leave the output array untouched"), UntouchedTransforms.Num(), 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSSphereFillTest, "RS.Combat.SphereFill", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSSphereFillTest::RunTest(const FString& Parameters)
{
	FRSCombatShape Donut;
	Donut.Type = ERSCombatShapeType::Sphere;
	Donut.Radius = 700.0f;
	Donut.InnerRadius = 300.0f;

	constexpr float Spacing = 100.0f;
	const FTransform DonutTransform(FRotator(0.0f, -20.0f, 0.0f), FVector(-500.0f, 900.0f, 120.0f));

	TArray<FTransform> FillTransforms;
	TestTrue(TEXT("Valid donut builds fill transforms"), URSCombatFunctionLibrary::BuildShapeFillTransforms(Donut, DonutTransform, Spacing, FillTransforms));
	TestTrue(TEXT("Valid donut produces at least one transform"), FillTransforms.Num() > 0);

	// 도넛의 빈 가운데에 연출이 생기면 안전지대가 위험해 보이므로 모든 칸이 두 반지름 사이인지 확인합니다
	const FVector DonutCenter = DonutTransform.GetLocation();

	bool bAllInsideDonut = true;
	bool bAllKeepPlane = true;
	bool bAllKeepRotation = true;
	for (const FTransform& FillTransform : FillTransforms)
	{
		const FVector FillLocation = FillTransform.GetLocation();
		const float HorizontalDistance = FVector::Dist2D(FillLocation, DonutCenter);
		if (HorizontalDistance < Donut.InnerRadius || HorizontalDistance > Donut.Radius)
		{
			bAllInsideDonut = false;
		}

		if (!FMath::IsNearlyEqual(FillLocation.Z, DonutCenter.Z))
		{
			bAllKeepPlane = false;
		}

		if (!FillTransform.GetRotation().Equals(DonutTransform.GetRotation()))
		{
			bAllKeepRotation = false;
		}
	}

	TestTrue(TEXT("Every fill transform stays between both radii"), bAllInsideDonut);
	TestTrue(TEXT("Every fill transform keeps the donut plane height"), bAllKeepPlane);
	TestTrue(TEXT("Every fill transform inherits the donut rotation"), bAllKeepRotation);

	// 안쪽 반지름이 0이면 가운데까지 채우므로 같은 바깥 반지름에서 도넛보다 칸이 많아야 합니다
	FRSCombatShape FullCircle = Donut;
	FullCircle.InnerRadius = 0.0f;

	TArray<FTransform> FullCircleTransforms;
	TestTrue(TEXT("Full circle builds fill transforms"), URSCombatFunctionLibrary::BuildShapeFillTransforms(FullCircle, DonutTransform, Spacing, FullCircleTransforms));
	TestTrue(TEXT("Full circle fills more cells than the donut"), FullCircleTransforms.Num() > FillTransforms.Num());

	// 간격이 범위보다 넓어도 연출이 사라지면 안 되므로 두 반지름 가운데 한 개로 대신하는지 확인합니다
	TArray<FTransform> SingleFillTransforms;
	TestTrue(TEXT("Spacing wider than the radius builds fill transforms"), URSCombatFunctionLibrary::BuildShapeFillTransforms(Donut, DonutTransform, Donut.Radius * 2.0f, SingleFillTransforms));
	TestEqual(TEXT("Spacing wider than the radius fills exactly one cell"), SingleFillTransforms.Num(), 1);

	if (SingleFillTransforms.Num() == 1)
	{
		const float SingleFillDistance = FVector::Dist2D(SingleFillTransforms[0].GetLocation(), DonutCenter);
		TestTrue(TEXT("The single fill transform sits between both radii"), SingleFillDistance >= Donut.InnerRadius && SingleFillDistance <= Donut.Radius);
		TestTrue(TEXT("The single fill transform inherits the donut rotation"), SingleFillTransforms[0].GetRotation().Equals(DonutTransform.GetRotation()));
	}

	// 잘못된 입력에서는 호출자가 이전 결과를 그대로 쓰지 않도록 실패를 알리고 배열을 건드리지 않습니다
	TArray<FTransform> UntouchedDonutTransforms;
	UntouchedDonutTransforms.Add(FTransform::Identity);

	TestFalse(TEXT("Zero spacing fails"), URSCombatFunctionLibrary::BuildShapeFillTransforms(Donut, DonutTransform, 0.0f, UntouchedDonutTransforms));

	FRSCombatShape InvalidDonut;
	InvalidDonut.Type = ERSCombatShapeType::Sphere;
	InvalidDonut.Radius = 200.0f;
	InvalidDonut.InnerRadius = 400.0f;
	TestFalse(TEXT("Inner radius larger than the outer radius fails"), URSCombatFunctionLibrary::BuildShapeFillTransforms(InvalidDonut, DonutTransform, Spacing, UntouchedDonutTransforms));

	// 간격이 과도하게 좁으면 순회 자체가 폭주하므로 채우기 전에 상한으로 막습니다
	TestFalse(TEXT("Runaway cell count fails"), URSCombatFunctionLibrary::BuildShapeFillTransforms(Donut, DonutTransform, 1.0f, UntouchedDonutTransforms));

	TestEqual(TEXT("Failed calls leave the output array untouched"), UntouchedDonutTransforms.Num(), 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSActorGroundLocationTest, "RS.Combat.ActorGroundLocation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSActorGroundLocationTest::RunTest(const FString& Parameters)
{
	// 잘못된 입력에서 호출자가 이전 값을 그대로 쓰지 않도록 실패를 알리고 출력을 건드리지 않습니다
	FVector UntouchedGroundLocation(1.0f, 2.0f, 3.0f);
	TestFalse(TEXT("A null actor fails"), URSCombatFunctionLibrary::TryGetActorGroundLocation(nullptr, UntouchedGroundLocation));
	TestEqual(TEXT("A failed call leaves the output untouched"), UntouchedGroundLocation, FVector(1.0f, 2.0f, 3.0f));

	const FName WorldName = MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("RSActorGroundLocationTestWorld"), EUniqueObjectNameOptions::GloballyUnique);
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false, WorldName, GetTransientPackage());
	TestWorld->AddToRoot();
	WorldContext.SetCurrentWorld(TestWorld);
	TestWorld->InitializeActorsForPlay(FURL());

	constexpr float CapsuleHalfHeight = 120.0f;
	const FVector CharacterLocation(300.0f, -200.0f, 500.0f);

	ACharacter* TestCharacter = TestWorld->SpawnActor<ACharacter>();
	TestCharacter->GetCapsuleComponent()->SetCapsuleSize(40.0f, CapsuleHalfHeight);
	TestCharacter->SetActorLocation(CharacterLocation);

	FVector GroundLocation = FVector::ZeroVector;
	TestTrue(TEXT("A character succeeds"), URSCombatFunctionLibrary::TryGetActorGroundLocation(TestCharacter, GroundLocation));
	TestEqual(TEXT("A character grounds at its capsule bottom"), GroundLocation, CharacterLocation - FVector(0.0f, 0.0f, CapsuleHalfHeight));

	// 크기를 키운 보스는 Scaled Half Height를 읽어야 바닥에 닿으므로 Scale을 무시하지 않는지 확인합니다
	constexpr float CapsuleScale = 2.0f;
	TestCharacter->SetActorScale3D(FVector(CapsuleScale));
	TestTrue(TEXT("A scaled character succeeds"), URSCombatFunctionLibrary::TryGetActorGroundLocation(TestCharacter, GroundLocation));
	TestEqual(TEXT("A scaled character grounds at its scaled capsule bottom"), GroundLocation, CharacterLocation - FVector(0.0f, 0.0f, CapsuleHalfHeight * CapsuleScale));

	// 캡슐이 없는 연출 액터도 같은 계약으로 바닥을 얻어야 호출처가 타입을 나누지 않습니다
	const FVector TestActorLocation(10.0f, 20.0f, 30.0f);
	ARSCombatShapeTestActor* CapsulelessActor = TestWorld->SpawnActor<ARSCombatShapeTestActor>();
	CapsulelessActor->SetActorLocation(TestActorLocation);

	TestTrue(TEXT("An actor without a capsule succeeds"), URSCombatFunctionLibrary::TryGetActorGroundLocation(CapsulelessActor, GroundLocation));
	TestEqual(TEXT("An actor without a capsule grounds by its simple collision"), GroundLocation, TestActorLocation - FVector(0.0f, 0.0f, 1.0f));

	TestWorld->RemoveFromRoot();
	GEngine->DestroyWorldContext(TestWorld);
	TestWorld->DestroyWorld(false);

	return true;
}

#endif
