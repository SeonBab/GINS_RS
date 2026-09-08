#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "RSAttackTelegraphComponent.h"
#include "Components/DecalComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSAttackTelegraphComponentTest, "RS.Combat.Telegraph.Component", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSAttackTelegraphComponentTest::RunTest(const FString& Parameters)
{
	const FName WorldName = MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("RSAttackTelegraphTestWorld"), EUniqueObjectNameOptions::GloballyUnique);
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false, WorldName, GetTransientPackage());
	TestWorld->AddToRoot();
	WorldContext.SetCurrentWorld(TestWorld);
	TestWorld->InitializeActorsForPlay(FURL());

	AActor* Owner = TestWorld->SpawnActor<AActor>();
	URSAttackTelegraphComponent* TelegraphComp = NewObject<URSAttackTelegraphComponent>(Owner);
	Owner->AddInstanceComponent(TelegraphComp);

	UMaterialInterface* TelegraphMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Resource/Telegraph/M_AttackTelegraph.M_AttackTelegraph"));
	FObjectProperty* DecalMaterialProperty = FindFProperty<FObjectProperty>(URSAttackTelegraphComponent::StaticClass(), TEXT("DecalMaterial"));
	TestNotNull(TEXT("Telegraph material asset"), TelegraphMaterial);
	TestNotNull(TEXT("DecalMaterial property"), DecalMaterialProperty);
	if (TelegraphMaterial && DecalMaterialProperty)
	{
		DecalMaterialProperty->SetObjectPropertyValue_InContainer(TelegraphComp, TelegraphMaterial);
	}
	TelegraphComp->RegisterComponent();

	FRSTelegraphPresentation Presentation;
	Presentation.HoldDuration = 10.0f;
	Presentation.Opacity = 0.4f;
	Presentation.FillDuration = 2.0f;

	FRSCombatShape Sphere;
	Sphere.Type = ERSCombatShapeType::Sphere;
	Sphere.Radius = 80.0f;
	Sphere.InnerRadius = 40.0f;
	TelegraphComp->ShowShape(Sphere, FTransform::Identity, Presentation);

	TArray<UDecalComponent*> Decals;
	Owner->GetComponents<UDecalComponent>(Decals);
	TestEqual(TEXT("Sphere creates one pooled decal"), Decals.Num(), 1);

	UDecalComponent* PooledDecal = Decals.IsEmpty() ? nullptr : Decals[0];
	if (PooledDecal)
	{
		TestEqual(TEXT("Sphere uses Radius as decal half size Y"), PooledDecal->DecalSize.Y, static_cast<double>(Sphere.Radius));
		TestEqual(TEXT("Sphere uses Radius as decal half size Z"), PooledDecal->DecalSize.Z, static_cast<double>(Sphere.Radius));

		UMaterialInstanceDynamic* MaterialInstance = Cast<UMaterialInstanceDynamic>(PooledDecal->GetDecalMaterial());
		TestNotNull(TEXT("Telegraph uses a dynamic material instance"), MaterialInstance);
		if (MaterialInstance)
		{
			TestEqual(TEXT("Presentation sets a fixed opacity without fading"), MaterialInstance->K2_GetScalarParameterValue(TEXT("Alpha")), Presentation.Opacity);
			TestEqual(TEXT("Fill starts at the apex or center"), MaterialInstance->K2_GetScalarParameterValue(TEXT("Fill")), 0.0f);
			TestEqual(TEXT("Sphere selects the existing radial mask"), MaterialInstance->K2_GetScalarParameterValue(TEXT("UseConeMask")), 0.0f);
			TestEqual(TEXT("Sphere resets cone angle parameter"), MaterialInstance->K2_GetScalarParameterValue(TEXT("ConeHalfAngleCos")), 1.0f);
			TestEqual(TEXT("Sphere sets its inner ratio"), MaterialInstance->K2_GetScalarParameterValue(TEXT("InnerRatio")), 0.5f);
		}
	}

	TelegraphComp->TickComponent(Presentation.HoldDuration, LEVELTICK_All, &TelegraphComp->PrimaryComponentTick);
	TestFalse(TEXT("Presentation releases the decal immediately at HoldDuration"), PooledDecal && PooledDecal->IsVisible());

	TelegraphComp->HideAllShapes();

	FRSCombatShape Cone;
	Cone.Type = ERSCombatShapeType::Cone;
	Cone.Range = 320.0f;
	Cone.Angle = 90.0f;
	const FVector ConeApex(120.0f, -75.0f, 10.0f);
	const FTransform ConeTransform(FRotator(0.0f, 90.0f, 0.0f), ConeApex);
	TelegraphComp->ShowShape(Cone, ConeTransform, Presentation);

	Decals.Reset();
	Owner->GetComponents<UDecalComponent>(Decals);
	TestEqual(TEXT("Cone reuses the existing pooled decal"), Decals.Num(), 1);
	if (!Decals.IsEmpty())
	{
		UDecalComponent* ConeDecal = Decals[0];
		TestEqual(TEXT("Cone uses Range as decal half size Y"), ConeDecal->DecalSize.Y, static_cast<double>(Cone.Range));
		TestEqual(TEXT("Cone uses Range as decal half size Z"), ConeDecal->DecalSize.Z, static_cast<double>(Cone.Range));
		TestTrue(TEXT("Cone keeps its apex at the decal center"), ConeDecal->GetComponentLocation().Equals(ConeApex));
		TestTrue(TEXT("Decal local Z used by material U aligns with shape forward"), ConeDecal->GetUpVector().Equals(ConeTransform.GetUnitAxis(EAxis::X)));

		UMaterialInstanceDynamic* MaterialInstance = Cast<UMaterialInstanceDynamic>(ConeDecal->GetDecalMaterial());
		if (MaterialInstance)
		{
			TestEqual(TEXT("Cone selects the cone mask"), MaterialInstance->K2_GetScalarParameterValue(TEXT("UseConeMask")), 1.0f);
			TestTrue(TEXT("Cone receives the cosine of its half angle"), FMath::IsNearlyEqual(MaterialInstance->K2_GetScalarParameterValue(TEXT("ConeHalfAngleCos")), FMath::InvSqrt(2.0f)));
			TestEqual(TEXT("Cone resets the radial inner ratio"), MaterialInstance->K2_GetScalarParameterValue(TEXT("InnerRatio")), 0.0f);
		}
	}

	TelegraphComp->HideAllShapes();
	Cone.Range = 0.0f;
	TelegraphComp->ShowShape(Cone, ConeTransform, Presentation);
	TestFalse(TEXT("Invalid cone does not reactivate the pooled decal"), PooledDecal && PooledDecal->IsVisible());

	TelegraphComp->UnregisterComponent();
	TestWorld->RemoveFromRoot();
	GEngine->DestroyWorldContext(TestWorld);
	TestWorld->DestroyWorld(false);

	return true;
}

#endif
