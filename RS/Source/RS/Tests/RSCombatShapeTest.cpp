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

	FRSCombatShape SectorValidationShape;
	SectorValidationShape.Type = ERSCombatShapeType::AnnularSector;

	// 바깥 반지름 기본값 0은 아직 정하지 않았다는 뜻이라 검증에서 걸립니다
	TestFalse(TEXT("A sector without an outer radius is rejected"), SectorValidationShape.IsDataValid());

	SectorValidationShape.OuterRadius = 100.0f;

	FString ValidationError;
	TestTrue(TEXT("A full circle sector is valid"), SectorValidationShape.IsDataValid(&ValidationError));
	TestTrue(TEXT("Valid sector data has no validation error"), ValidationError.IsEmpty());

	SectorValidationShape.InnerRadius = 100.0f;
	TestFalse(TEXT("A sector rejects an inner radius at its outer boundary"), SectorValidationShape.IsDataValid());
	SectorValidationShape.InnerRadius = 0.0f;

	SectorValidationShape.SweepAngleDegrees = 0.0f;
	TestFalse(TEXT("A sector rejects a zero sweep"), SectorValidationShape.IsDataValid());

	// 180도 상한은 Cone 시절의 정책이었고 환형 부채꼴은 한 바퀴까지 표현합니다
	SectorValidationShape.SweepAngleDegrees = 270.0f;
	TestTrue(TEXT("A sector accepts a sweep beyond half a turn"), SectorValidationShape.IsDataValid());
	SectorValidationShape.SweepAngleDegrees = -90.0f;
	TestTrue(TEXT("A sector accepts a negative sweep as its direction"), SectorValidationShape.IsDataValid());
	SectorValidationShape.SweepAngleDegrees = 360.01f;
	TestFalse(TEXT("A sector rejects a sweep beyond a full turn"), SectorValidationShape.IsDataValid());

	// 전방 대칭 90도 부채꼴이라 예전 Cone과 같은 범위를 덮습니다
	SectorValidationShape.SweepAngleDegrees = 90.0f;
	SectorValidationShape.StartYawOffset = -45.0f;

	const FTransform SectorTransform(FRotator::ZeroRotator, FVector::ZeroVector);
	const FRSAnnularSectorBounds ValidationBounds = SectorValidationShape.GetAnnularSectorBounds();
	TestTrue(TEXT("A pivot touching sector includes its own pivot"), URSCombatFunctionLibrary::IsLocationInsideAnnularSector(SectorTransform, ValidationBounds, FVector(0.0f, 0.0f, 500.0f)));
	TestTrue(TEXT("A sector includes a location well inside it"), URSCombatFunctionLibrary::IsLocationInsideAnnularSector(SectorTransform, ValidationBounds, FVector(50.0f, 0.0f, 0.0f)));

	// 회전과 이동이 없어 왕복 변환에 오차가 없으므로 바깥 경계 값을 그대로 비교할 수 있습니다
	TestFalse(TEXT("A sector excludes its open outer boundary"), URSCombatFunctionLibrary::IsLocationInsideAnnularSector(SectorTransform, ValidationBounds, FVector(100.0f, 0.0f, 0.0f)));
	TestFalse(TEXT("A sector excludes beyond its outer radius"), URSCombatFunctionLibrary::IsLocationInsideAnnularSector(SectorTransform, ValidationBounds, FVector(100.1f, 0.0f, 0.0f)));
	TestFalse(TEXT("A sector excludes a location outside its angle"), URSCombatFunctionLibrary::IsLocationInsideAnnularSector(SectorTransform, ValidationBounds, FVector(50.0f, 51.0f, 0.0f)));
	TestFalse(TEXT("A sector excludes locations behind it"), URSCombatFunctionLibrary::IsLocationInsideAnnularSector(SectorTransform, ValidationBounds, FVector(-10.0f, 0.0f, 0.0f)));

	const FTransform RotatedSectorTransform(FRotator(0.0f, 90.0f, 0.0f), FVector::ZeroVector);
	TestTrue(TEXT("A sector follows transform yaw"), URSCombatFunctionLibrary::IsLocationInsideAnnularSector(RotatedSectorTransform, ValidationBounds, FVector(0.0f, 50.0f, 0.0f)));

	const FName WorldName = MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("RSCombatShapeTestWorld"), EUniqueObjectNameOptions::GloballyUnique);
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false, WorldName, GetTransientPackage());
	TestWorld->AddToRoot();
	WorldContext.SetCurrentWorld(TestWorld);
	TestWorld->InitializeActorsForPlay(FURL());
	AActor* Attacker = TestWorld->SpawnActor<AActor>();
	ARSCombatShapeTestActor* Target = TestWorld->SpawnActor<ARSCombatShapeTestActor>();

	Target->SetActorLocation(FVector(50.0f, 0.0f, 0.0f));
	TestTrue(TEXT("FindTargetsInShape includes a target inside the sector"), RSCombatShapeTest::ContainsTarget(*Attacker, SectorValidationShape, SectorTransform, *Target));
	Target->GetAbilitySystemComponent()->AddLooseGameplayTag(RSGameplayTags::State_Dead);
	TestFalse(TEXT("FindTargetsInShape keeps excluding dead targets"), RSCombatShapeTest::ContainsTarget(*Attacker, SectorValidationShape, SectorTransform, *Target));
	Target->GetAbilitySystemComponent()->RemoveLooseGameplayTag(RSGameplayTags::State_Dead);
	Target->SetActorLocation(FVector(100.0f, 0.0f, 0.0f));
	TestFalse(TEXT("FindTargetsInShape excludes the open outer boundary"), RSCombatShapeTest::ContainsTarget(*Attacker, SectorValidationShape, SectorTransform, *Target));
	Target->SetActorLocation(FVector(-10.0f, 0.0f, 0.0f));
	TestFalse(TEXT("FindTargetsInShape applies the sector angle filter"), RSCombatShapeTest::ContainsTarget(*Attacker, SectorValidationShape, SectorTransform, *Target));

	FRSCombatShape Sphere;
	Sphere.Type = ERSCombatShapeType::AnnularSector;
	Sphere.OuterRadius = 100.0f;
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
	Cone.Type = ERSCombatShapeType::AnnularSector;
	Cone.OuterRadius = 600.0f;
	Cone.StartYawOffset = -45.0f;
	Cone.SweepAngleDegrees = 90.0f;
	const FRSAnnularSectorBounds ConeBounds = Cone.GetAnnularSectorBounds();

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
		if (!URSCombatFunctionLibrary::IsLocationInsideAnnularSector(ConeTransform, ConeBounds, FillLocation))
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
	TestTrue(TEXT("Spacing wider than the range builds fill transforms"), URSCombatFunctionLibrary::BuildShapeFillTransforms(Cone, ConeTransform, Cone.OuterRadius * 2.0f, SingleFillTransforms));
	TestEqual(TEXT("Spacing wider than the range fills exactly one cell"), SingleFillTransforms.Num(), 1);

	if (SingleFillTransforms.Num() == 1)
	{
		const FVector ExpectedCenter = ConeApex + ConeForward * (Cone.OuterRadius * 0.5f);
		TestTrue(TEXT("The single fill transform sits at the cone center"), SingleFillTransforms[0].GetLocation().Equals(ExpectedCenter));
		TestTrue(TEXT("The single fill transform stays inside the cone"), URSCombatFunctionLibrary::IsLocationInsideAnnularSector(ConeTransform, ConeBounds, SingleFillTransforms[0].GetLocation()));
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
	InvalidCone.Type = ERSCombatShapeType::AnnularSector;
	InvalidCone.OuterRadius = 0.0f;
	InvalidCone.SweepAngleDegrees = 90.0f;
	TestFalse(TEXT("A sector without an outer radius fails"), URSCombatFunctionLibrary::BuildShapeFillTransforms(InvalidCone, ConeTransform, Spacing, UntouchedTransforms));

	// 간격이 과도하게 좁으면 순회 자체가 폭주하므로 채우기 전에 상한으로 막습니다
	TestFalse(TEXT("Runaway cell count fails"), URSCombatFunctionLibrary::BuildShapeFillTransforms(Cone, ConeTransform, 1.0f, UntouchedTransforms));

	TestEqual(TEXT("Failed calls leave the output array untouched"), UntouchedTransforms.Num(), 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSSphereFillTest, "RS.Combat.SphereFill", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSSphereFillTest::RunTest(const FString& Parameters)
{
	FRSCombatShape Donut;
	Donut.Type = ERSCombatShapeType::AnnularSector;
	Donut.OuterRadius = 700.0f;
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
		if (HorizontalDistance < Donut.InnerRadius || HorizontalDistance > Donut.OuterRadius)
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
	TestTrue(TEXT("Spacing wider than the radius builds fill transforms"), URSCombatFunctionLibrary::BuildShapeFillTransforms(Donut, DonutTransform, Donut.OuterRadius * 2.0f, SingleFillTransforms));
	TestEqual(TEXT("Spacing wider than the radius fills exactly one cell"), SingleFillTransforms.Num(), 1);

	if (SingleFillTransforms.Num() == 1)
	{
		const float SingleFillDistance = FVector::Dist2D(SingleFillTransforms[0].GetLocation(), DonutCenter);
		TestTrue(TEXT("The single fill transform sits between both radii"), SingleFillDistance >= Donut.InnerRadius && SingleFillDistance <= Donut.OuterRadius);
		TestTrue(TEXT("The single fill transform inherits the donut rotation"), SingleFillTransforms[0].GetRotation().Equals(DonutTransform.GetRotation()));
	}

	// 잘못된 입력에서는 호출자가 이전 결과를 그대로 쓰지 않도록 실패를 알리고 배열을 건드리지 않습니다
	TArray<FTransform> UntouchedDonutTransforms;
	UntouchedDonutTransforms.Add(FTransform::Identity);

	TestFalse(TEXT("Zero spacing fails"), URSCombatFunctionLibrary::BuildShapeFillTransforms(Donut, DonutTransform, 0.0f, UntouchedDonutTransforms));

	FRSCombatShape InvalidDonut;
	InvalidDonut.Type = ERSCombatShapeType::AnnularSector;
	InvalidDonut.OuterRadius = 200.0f;
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


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSPatternStartRadiusTest, "RS.Combat.PatternStartRadius", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSPatternStartRadiusTest::RunTest(const FString& Parameters)
{
	// 잘못된 입력에서 호출자가 이전 값을 그대로 쓰지 않도록 실패를 알리고 출력을 건드리지 않습니다
	float UntouchedRadius = 7.0f;
	TestFalse(TEXT("A null actor fails"), URSCombatFunctionLibrary::TryGetActorHorizontalRadius(nullptr, UntouchedRadius));
	TestEqual(TEXT("A failed call leaves the output untouched"), UntouchedRadius, 7.0f);

	const FName WorldName = MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("RSPatternStartRadiusTestWorld"), EUniqueObjectNameOptions::GloballyUnique);
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false, WorldName, GetTransientPackage());
	TestWorld->AddToRoot();
	WorldContext.SetCurrentWorld(TestWorld);
	TestWorld->InitializeActorsForPlay(FURL());

	constexpr float CapsuleRadius = 40.0f;

	ACharacter* TestCharacter = TestWorld->SpawnActor<ACharacter>();
	TestCharacter->GetCapsuleComponent()->SetCapsuleSize(CapsuleRadius, 120.0f);

	float HorizontalRadius = 0.0f;
	TestTrue(TEXT("A character succeeds"), URSCombatFunctionLibrary::TryGetActorHorizontalRadius(TestCharacter, HorizontalRadius));
	TestEqual(TEXT("A character reports its capsule radius"), HorizontalRadius, CapsuleRadius);

	// 크기를 키운 보스의 시작 반경이 함께 커져야 하므로 Scale을 무시하지 않는지 확인합니다
	constexpr float CapsuleScale = 2.0f;
	TestCharacter->SetActorScale3D(FVector(CapsuleScale));
	TestTrue(TEXT("A scaled character succeeds"), URSCombatFunctionLibrary::TryGetActorHorizontalRadius(TestCharacter, HorizontalRadius));
	TestEqual(TEXT("A scaled character reports its scaled capsule radius"), HorizontalRadius, CapsuleRadius * CapsuleScale);

	// 캡슐이 없는 액터도 같은 계약으로 받아야 호출처가 액터 타입을 나누지 않습니다
	ARSCombatShapeTestActor* CapsulelessActor = TestWorld->SpawnActor<ARSCombatShapeTestActor>();
	TestTrue(TEXT("An actor without a capsule succeeds"), URSCombatFunctionLibrary::TryGetActorHorizontalRadius(CapsulelessActor, HorizontalRadius));
	TestEqual(TEXT("An actor without a capsule reports its simple collision radius"), HorizontalRadius, CapsulelessActor->GetSimpleCollisionRadius());

	TestWorld->RemoveFromRoot();
	GEngine->DestroyWorldContext(TestWorld);
	TestWorld->DestroyWorld(false);

	// 에셋이 적은 안쪽 경계가 하한보다 크면 에셋 값이 이겨야 합니다. 하한은 바닥이지 덮어쓰기가 아닙니다
	FRSCombatShape WideDonut;
	WideDonut.Type = ERSCombatShapeType::AnnularSector;
	WideDonut.OuterRadius = 800.0f;
	WideDonut.InnerRadius = 300.0f;
	TestTrue(TEXT("A floor below the authored inner radius keeps area"), URSCombatFunctionLibrary::TryApplyMinimumInnerRadius(WideDonut, 100.0f));
	TestEqual(TEXT("A floor below the authored inner radius changes nothing"), WideDonut.InnerRadius, 300.0f);

	// 0으로 적힌 링이 보스 캡슐 안쪽을 덮지 않게 하는 것이 이 계약의 목적입니다
	FRSCombatShape FullDisc;
	FullDisc.Type = ERSCombatShapeType::AnnularSector;
	FullDisc.OuterRadius = 800.0f;
	TestTrue(TEXT("A floor above the authored inner radius keeps area"), URSCombatFunctionLibrary::TryApplyMinimumInnerRadius(FullDisc, 100.0f));
	TestEqual(TEXT("A floor above the authored inner radius raises the inner radius"), FullDisc.InnerRadius, 100.0f);

	// 하한이 없거나 의미 없는 값이면 형상을 그대로 두고 성공을 알립니다
	FRSCombatShape UnfloorredDisc;
	UnfloorredDisc.Type = ERSCombatShapeType::AnnularSector;
	UnfloorredDisc.OuterRadius = 800.0f;
	TestTrue(TEXT("A non-positive floor keeps area"), URSCombatFunctionLibrary::TryApplyMinimumInnerRadius(UnfloorredDisc, 0.0f));
	TestEqual(TEXT("A non-positive floor changes nothing"), UnfloorredDisc.InnerRadius, 0.0f);

	// 형상 전체가 보스 안에 있으면 남는 판정이 없으므로 호출자가 그 형상을 건너뛸 수 있어야 합니다
	FRSCombatShape SwallowedDisc;
	SwallowedDisc.Type = ERSCombatShapeType::AnnularSector;
	SwallowedDisc.OuterRadius = 80.0f;
	TestFalse(TEXT("A floor at or beyond the outer radius leaves no area"), URSCombatFunctionLibrary::TryApplyMinimumInnerRadius(SwallowedDisc, 80.0f));

	FRSCombatShape InvalidDisc;
	InvalidDisc.Type = ERSCombatShapeType::AnnularSector;
	InvalidDisc.OuterRadius = 0.0f;
	TestFalse(TEXT("An invalid shape leaves no area"), URSCombatFunctionLibrary::TryApplyMinimumInnerRadius(InvalidDisc, 100.0f));

	// 부채꼴도 같은 하한을 받으므로 보스 발밑에서 시작하지 않습니다
	FRSCombatShape Wedge;
	Wedge.Type = ERSCombatShapeType::AnnularSector;
	Wedge.OuterRadius = 800.0f;
	Wedge.StartYawOffset = -22.5f;
	Wedge.SweepAngleDegrees = 45.0f;
	TestTrue(TEXT("A sector keeps area"), URSCombatFunctionLibrary::TryApplyMinimumInnerRadius(Wedge, 100.0f));
	TestEqual(TEXT("A sector raises its inner radius to the floor"), Wedge.InnerRadius, 100.0f);
	TestEqual(TEXT("A sector keeps its angle"), Wedge.SweepAngleDegrees, 45.0f);

	// ArmSwing은 형상을 에셋으로 적지 않고 매 틱 경계를 계산해 내므로 하한 규칙을 경계 층에서 그대로 받아야 합니다
	FRSAnnularSectorBounds AuthoredBounds;
	AuthoredBounds.OuterRadius = 800.0f;
	AuthoredBounds.InnerRadius = 300.0f;
	AuthoredBounds.SweepAngleDegrees = 40.0f;
	TestTrue(TEXT("A floor below the calculated inner radius keeps area"), URSCombatFunctionLibrary::TryApplyMinimumInnerRadius(AuthoredBounds, 100.0f));
	TestEqual(TEXT("A floor below the calculated inner radius changes nothing"), AuthoredBounds.InnerRadius, 300.0f);

	FRSAnnularSectorBounds CenteredBounds;
	CenteredBounds.OuterRadius = 800.0f;
	CenteredBounds.SweepAngleDegrees = 40.0f;
	TestTrue(TEXT("A floor above the calculated inner radius keeps area"), URSCombatFunctionLibrary::TryApplyMinimumInnerRadius(CenteredBounds, 100.0f));
	TestEqual(TEXT("A floor above the calculated inner radius raises the inner radius"), CenteredBounds.InnerRadius, 100.0f);
	TestEqual(TEXT("A floored bound keeps its angle"), CenteredBounds.SweepAngleDegrees, 40.0f);

	FRSAnnularSectorBounds UnflooredBounds;
	UnflooredBounds.OuterRadius = 800.0f;
	UnflooredBounds.SweepAngleDegrees = 40.0f;
	TestTrue(TEXT("A non-positive floor keeps the bounds"), URSCombatFunctionLibrary::TryApplyMinimumInnerRadius(UnflooredBounds, 0.0f));
	TestEqual(TEXT("A non-positive floor changes nothing on the bounds"), UnflooredBounds.InnerRadius, 0.0f);

	// 면적이 남지 않는 순간을 호출자가 알아야 그 구간의 판정을 건너뛸 수 있습니다
	FRSAnnularSectorBounds SwallowedBounds;
	SwallowedBounds.OuterRadius = 80.0f;
	SwallowedBounds.SweepAngleDegrees = 40.0f;
	TestFalse(TEXT("A floor at or beyond the outer bound leaves no area"), URSCombatFunctionLibrary::TryApplyMinimumInnerRadius(SwallowedBounds, 80.0f));

	FRSAnnularSectorBounds InvalidBounds;
	InvalidBounds.SweepAngleDegrees = 40.0f;
	TestFalse(TEXT("Invalid bounds leave no area"), URSCombatFunctionLibrary::TryApplyMinimumInnerRadius(InvalidBounds, 100.0f));

	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSAnnularSectorTest, "RS.Combat.AnnularSector", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSAnnularSectorTest::RunTest(const FString& Parameters)
{
	// 회전과 이동을 함께 준 Transform으로 검사해야 로컬 변환을 건너뛴 구현이 통과하지 않습니다
	const FTransform SectorTransform(FRotator(0.0f, 90.0f, 0.0f), FVector(10.0f, 20.0f, 30.0f));
	const auto LocationAt = [&SectorTransform](float YawDegrees, float Radius)
		{
			return SectorTransform.TransformPosition(FRotator(0.0f, YawDegrees, 0.0f).Vector() * Radius);
		};

	FRSAnnularSectorBounds Bounds;
	Bounds.InnerRadius = 100.0f;
	Bounds.OuterRadius = 500.0f;
	Bounds.StartYawOffset = -45.0f;
	Bounds.SweepAngleDegrees = 90.0f;

	TestTrue(TEXT("A center location inside the sector is included"), URSCombatFunctionLibrary::IsLocationInsideAnnularSector(SectorTransform, Bounds, LocationAt(0.0f, 300.0f)));
	TestFalse(TEXT("A center location inside the inner radius is excluded"), URSCombatFunctionLibrary::IsLocationInsideAnnularSector(SectorTransform, Bounds, LocationAt(0.0f, 99.0f)));
	TestFalse(TEXT("A center location beyond the outer radius is excluded"), URSCombatFunctionLibrary::IsLocationInsideAnnularSector(SectorTransform, Bounds, LocationAt(0.0f, 501.0f)));
	TestFalse(TEXT("A center location outside the swept angle is excluded"), URSCombatFunctionLibrary::IsLocationInsideAnnularSector(SectorTransform, Bounds, LocationAt(60.0f, 300.0f)));

	// 평면 전투이므로 높이는 판정에 들어가지 않습니다
	TestTrue(TEXT("A flat sector ignores the target height"), URSCombatFunctionLibrary::IsLocationInsideAnnularSector(SectorTransform, Bounds, LocationAt(0.0f, 300.0f) + FVector(0.0f, 0.0f, 1000.0f)));

	// 회전과 이동이 없으면 왕복 변환에 오차가 없어 경계 값을 그대로 비교할 수 있습니다
	FRSAnnularSectorBounds ExactBounds;
	ExactBounds.InnerRadius = 100.0f;
	ExactBounds.OuterRadius = 500.0f;
	ExactBounds.StartYawOffset = -45.0f;
	ExactBounds.SweepAngleDegrees = 90.0f;

	TestTrue(TEXT("The inner boundary belongs to the sector"), URSCombatFunctionLibrary::IsLocationInsideAnnularSector(FTransform::Identity, ExactBounds, FVector(100.0f, 0.0f, 0.0f)));
	TestFalse(TEXT("The outer boundary does not belong to the sector"), URSCombatFunctionLibrary::IsLocationInsideAnnularSector(FTransform::Identity, ExactBounds, FVector(500.0f, 0.0f, 0.0f)));

	// 반개구간이 실제로 사는 이유입니다. 이어 붙는 두 형상이 경계를 공유해도 대상은 정확히 한쪽에만 걸려야 합니다
	// 경계 좌표에 부동소수점 오차가 있어도 두 형상이 같은 값을 보므로 이 단언은 흔들리지 않습니다
	FRSAnnularSectorBounds InnerRing;
	InnerRing.InnerRadius = 0.0f;
	InnerRing.OuterRadius = 600.0f;
	InnerRing.SweepAngleDegrees = 360.0f;

	FRSAnnularSectorBounds OuterRing;
	OuterRing.InnerRadius = 600.0f;
	OuterRing.OuterRadius = 800.0f;
	OuterRing.SweepAngleDegrees = 360.0f;

	const FVector SharedRadiusLocation = LocationAt(0.0f, 600.0f);
	const bool bInnerRingHit = URSCombatFunctionLibrary::IsLocationInsideAnnularSector(SectorTransform, InnerRing, SharedRadiusLocation);
	const bool bOuterRingHit = URSCombatFunctionLibrary::IsLocationInsideAnnularSector(SectorTransform, OuterRing, SharedRadiusLocation);
	TestTrue(TEXT("A target on a shared ring boundary is hit by exactly one ring"), bInnerRingHit != bOuterRingHit);
	TestTrue(TEXT("A shared ring boundary belongs to the outer ring"), bOuterRingHit);

	FRSAnnularSectorBounds FirstSlice;
	FirstSlice.OuterRadius = 800.0f;
	FirstSlice.StartYawOffset = 0.0f;
	FirstSlice.SweepAngleDegrees = 90.0f;

	FRSAnnularSectorBounds SecondSlice;
	SecondSlice.OuterRadius = 800.0f;
	SecondSlice.StartYawOffset = 90.0f;
	SecondSlice.SweepAngleDegrees = 90.0f;

	const FVector SharedAngleLocation = LocationAt(90.0f, 300.0f);
	const bool bFirstSliceHit = URSCombatFunctionLibrary::IsLocationInsideAnnularSector(SectorTransform, FirstSlice, SharedAngleLocation);
	const bool bSecondSliceHit = URSCombatFunctionLibrary::IsLocationInsideAnnularSector(SectorTransform, SecondSlice, SharedAngleLocation);
	TestTrue(TEXT("A target on a shared slice boundary is hit by exactly one slice"), bFirstSliceHit != bSecondSliceHit);
	TestTrue(TEXT("A shared slice boundary belongs to the slice that starts there"), bSecondSliceHit);

	// 360도는 시작 경계와 끝 경계가 같은 지점이라 반개구간을 그대로 적용하면 그 한 방향이 빠집니다
	FRSAnnularSectorBounds FullDonut;
	FullDonut.InnerRadius = 100.0f;
	FullDonut.OuterRadius = 800.0f;
	FullDonut.StartYawOffset = 35.0f;
	FullDonut.SweepAngleDegrees = 360.0f;
	TestTrue(TEXT("A full donut includes its own start boundary"), URSCombatFunctionLibrary::IsLocationInsideAnnularSector(SectorTransform, FullDonut, LocationAt(35.0f, 400.0f)));
	TestTrue(TEXT("A full donut includes the opposite direction"), URSCombatFunctionLibrary::IsLocationInsideAnnularSector(SectorTransform, FullDonut, LocationAt(-145.0f, 400.0f)));

	// 부호가 방향이므로 음수 Sweep은 시작 경계에서 반대쪽으로 펼쳐집니다
	FRSAnnularSectorBounds MirroredSlice;
	MirroredSlice.OuterRadius = 800.0f;
	MirroredSlice.StartYawOffset = 0.0f;
	MirroredSlice.SweepAngleDegrees = -90.0f;
	TestTrue(TEXT("A negative sweep covers its directed side"), URSCombatFunctionLibrary::IsLocationInsideAnnularSector(SectorTransform, MirroredSlice, LocationAt(-45.0f, 300.0f)));
	TestFalse(TEXT("A negative sweep excludes the opposite side"), URSCombatFunctionLibrary::IsLocationInsideAnnularSector(SectorTransform, MirroredSlice, LocationAt(45.0f, 300.0f)));

	// 안쪽 반지름이 0이면 중심에는 방향이 없으므로 반지름 검사만으로 판단합니다
	FRSAnnularSectorBounds PivotTouchingSlice;
	PivotTouchingSlice.OuterRadius = 500.0f;
	PivotTouchingSlice.StartYawOffset = 0.0f;
	PivotTouchingSlice.SweepAngleDegrees = 90.0f;
	TestTrue(TEXT("A pivot touching sector includes its own pivot"), URSCombatFunctionLibrary::IsLocationInsideAnnularSector(SectorTransform, PivotTouchingSlice, SectorTransform.GetLocation()));

	FRSAnnularSectorBounds HollowSlice = PivotTouchingSlice;
	HollowSlice.InnerRadius = 100.0f;
	TestFalse(TEXT("A hollow sector excludes the pivot"), URSCombatFunctionLibrary::IsLocationInsideAnnularSector(SectorTransform, HollowSlice, SectorTransform.GetLocation()));

	// 판정할 수 없는 경계는 조용히 비우지 않고 거부합니다
	FRSAnnularSectorBounds CollapsedBounds = Bounds;
	CollapsedBounds.OuterRadius = CollapsedBounds.InnerRadius;
	TestFalse(TEXT("An outer radius at the inner radius is rejected"), URSCombatFunctionLibrary::IsLocationInsideAnnularSector(SectorTransform, CollapsedBounds, LocationAt(0.0f, 300.0f)));

	FRSAnnularSectorBounds ZeroSweepBounds = Bounds;
	ZeroSweepBounds.SweepAngleDegrees = 0.0f;
	TestFalse(TEXT("A zero sweep is rejected"), URSCombatFunctionLibrary::IsLocationInsideAnnularSector(SectorTransform, ZeroSweepBounds, LocationAt(0.0f, 300.0f)));

	FRSAnnularSectorBounds OverSweptBounds = Bounds;
	OverSweptBounds.SweepAngleDegrees = 361.0f;
	TestFalse(TEXT("A sweep beyond a full turn is rejected"), URSCombatFunctionLibrary::IsLocationInsideAnnularSector(SectorTransform, OverSweptBounds, LocationAt(0.0f, 300.0f)));

	FRSAnnularSectorBounds NegativeInnerBounds = Bounds;
	NegativeInnerBounds.InnerRadius = -1.0f;
	TestFalse(TEXT("A negative inner radius is rejected"), URSCombatFunctionLibrary::IsLocationInsideAnnularSector(SectorTransform, NegativeInnerBounds, LocationAt(0.0f, 300.0f)));

	return true;
}

#endif
