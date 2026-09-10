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
	if (TelegraphMaterial)
	{
		if (DecalMaterialProperty)
		{
			DecalMaterialProperty->SetObjectPropertyValue_InContainer(TelegraphComp, TelegraphMaterial);
		}
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
			TestEqual(TEXT("Sphere selects the radial shape mode"), MaterialInstance->K2_GetScalarParameterValue(TEXT("ShapeMode")), 0.0f);
			TestEqual(TEXT("Sphere selects radial fill"), MaterialInstance->K2_GetScalarParameterValue(TEXT("FillMode")), 0.0f);
			TestEqual(TEXT("Sphere resets cone angle parameter"), MaterialInstance->K2_GetScalarParameterValue(TEXT("ConeHalfAngleCos")), 1.0f);
			TestEqual(TEXT("Sphere sets its inner ratio"), MaterialInstance->K2_GetScalarParameterValue(TEXT("InnerRatio")), 0.5f);
		}
	}

	TelegraphComp->TickComponent(Presentation.HoldDuration, LEVELTICK_All, &TelegraphComp->PrimaryComponentTick);
	TestFalse(TEXT("Presentation releases the decal immediately at HoldDuration"), PooledDecal && PooledDecal->IsVisible());

	TelegraphComp->HideAllShapes();

	FRSCombatShape FirstHandledSphere = Sphere;
	FirstHandledSphere.InnerRadius = 0.0f;
	FirstHandledSphere.Radius = 60.0f;
	FRSCombatShape SecondHandledSphere = FirstHandledSphere;
	SecondHandledSphere.Radius = 100.0f;
	const int32 FirstSphereHandle = TelegraphComp->ShowShapeWithHandle(FirstHandledSphere, FTransform(FVector(100.0f, 0.0f, 0.0f)), Presentation);
	const int32 SecondSphereHandle = TelegraphComp->ShowShapeWithHandle(SecondHandledSphere, FTransform(FVector(-100.0f, 0.0f, 0.0f)), Presentation);
	TestTrue(TEXT("General shapes return distinct valid handles"), FirstSphereHandle != INDEX_NONE && SecondSphereHandle != INDEX_NONE && FirstSphereHandle != SecondSphereHandle);

	Decals.Reset();
	Owner->GetComponents<UDecalComponent>(Decals);
	int32 VisibleDecalCount = 0;
	for (const UDecalComponent* Decal : Decals)
	{
		VisibleDecalCount += Decal && Decal->IsVisible() ? 1 : 0;
	}
	TestEqual(TEXT("Two handled shapes remain visible together"), VisibleDecalCount, 2);

	TelegraphComp->HideShape(FirstSphereHandle);
	VisibleDecalCount = 0;
	for (const UDecalComponent* Decal : Decals)
	{
		VisibleDecalCount += Decal && Decal->IsVisible() ? 1 : 0;
	}
	TestEqual(TEXT("Hiding one general shape preserves the other"), VisibleDecalCount, 1);

	TelegraphComp->HideAllShapes();
	VisibleDecalCount = 0;
	for (const UDecalComponent* Decal : Decals)
	{
		VisibleDecalCount += Decal && Decal->IsVisible() ? 1 : 0;
	}
	TestEqual(TEXT("Hide all releases every handled shape"), VisibleDecalCount, 0);

	FRSCombatShape Cone;
	Cone.Type = ERSCombatShapeType::Cone;
	Cone.Range = 320.0f;
	Cone.Angle = 90.0f;
	const FVector ConeApex(120.0f, -75.0f, 10.0f);
	const FTransform ConeTransform(FRotator(0.0f, 90.0f, 0.0f), ConeApex);
	TelegraphComp->ShowShape(Cone, ConeTransform, Presentation);

	Decals.Reset();
	Owner->GetComponents<UDecalComponent>(Decals);
	TestEqual(TEXT("Cone reuses the existing decal pool"), Decals.Num(), 2);
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
			TestEqual(TEXT("Cone selects the cone shape mode"), MaterialInstance->K2_GetScalarParameterValue(TEXT("ShapeMode")), 1.0f);
			TestEqual(TEXT("Cone selects radial fill"), MaterialInstance->K2_GetScalarParameterValue(TEXT("FillMode")), 0.0f);
			TestTrue(TEXT("Cone receives the cosine of its half angle"), FMath::IsNearlyEqual(MaterialInstance->K2_GetScalarParameterValue(TEXT("ConeHalfAngleCos")), FMath::InvSqrt(2.0f)));
			TestEqual(TEXT("Cone resets the radial inner ratio"), MaterialInstance->K2_GetScalarParameterValue(TEXT("InnerRatio")), 0.0f);
		}
	}

	TelegraphComp->HideAllShapes();
	Cone.Range = 0.0f;
	TelegraphComp->ShowShape(Cone, ConeTransform, Presentation);
	TestFalse(TEXT("Invalid cone does not reactivate the pooled decal"), PooledDecal && PooledDecal->IsVisible());

	FRSAnnularSectorTelegraphDefinition AnnularSector;
	AnnularSector.InnerRadius = 50.0f;
	AnnularSector.OuterRadius = 200.0f;
	AnnularSector.StartYawOffset = -60.0f;
	AnnularSector.SweepAngleDegrees = 120.0f;
	const FVector AnnularSectorOrigin(80.0f, 25.0f, 10.0f);
	const FTransform AnnularSectorTransform(FRotator(0.0f, 30.0f, 0.0f), AnnularSectorOrigin);
	const int32 AnnularSectorHandle = TelegraphComp->ShowAnnularSector(AnnularSector, AnnularSectorTransform);
	TestTrue(TEXT("Valid annular sector returns a handle"), AnnularSectorHandle != INDEX_NONE);
	TestTrue(TEXT("Annular sector reuses and shows the pooled decal"), PooledDecal && PooledDecal->IsVisible());
	if (PooledDecal)
	{
		TestEqual(TEXT("Annular sector uses OuterRadius as decal half size Y"), PooledDecal->DecalSize.Y, static_cast<double>(AnnularSector.OuterRadius));
		TestEqual(TEXT("Annular sector uses OuterRadius as decal half size Z"), PooledDecal->DecalSize.Z, static_cast<double>(AnnularSector.OuterRadius));
		TestTrue(TEXT("Annular sector stays at the locked origin"), PooledDecal->GetComponentLocation().Equals(AnnularSectorOrigin));
		const FVector ExpectedStartDirection = FRotator(0.0f, -30.0f, 0.0f).Vector();
		TestTrue(TEXT("Annular sector local material forward aligns with its start angle"), PooledDecal->GetUpVector().Equals(ExpectedStartDirection));

		UMaterialInstanceDynamic* MaterialInstance = Cast<UMaterialInstanceDynamic>(PooledDecal->GetDecalMaterial());
		if (MaterialInstance)
		{
			TestEqual(TEXT("Annular sector selects the angular sector shape mode"), MaterialInstance->K2_GetScalarParameterValue(TEXT("ShapeMode")), 2.0f);
			TestEqual(TEXT("Annular sector selects angular fill"), MaterialInstance->K2_GetScalarParameterValue(TEXT("FillMode")), 1.0f);
			TestEqual(TEXT("Annular sector sets its inner ratio"), MaterialInstance->K2_GetScalarParameterValue(TEXT("InnerRatio")), 0.25f);
			TestEqual(TEXT("Annular sector sets its signed sweep"), MaterialInstance->K2_GetScalarParameterValue(TEXT("SweepAngleDegrees")), AnnularSector.SweepAngleDegrees);
			TestEqual(TEXT("Annular sector starts with no directional fill"), MaterialInstance->K2_GetScalarParameterValue(TEXT("Fill")), 0.0f);
			TestTrue(TEXT("External fill accepts a valid handle"), TelegraphComp->SetExternalFill(AnnularSectorHandle, 0.75f));
			TestEqual(TEXT("External fill updates the material directly"), MaterialInstance->K2_GetScalarParameterValue(TEXT("Fill")), 0.75f);
		}
	}

	TelegraphComp->TickComponent(Presentation.HoldDuration, LEVELTICK_All, &TelegraphComp->PrimaryComponentTick);
	TestTrue(TEXT("Externally driven annular sector does not expire by elapsed time"), PooledDecal && PooledDecal->IsVisible());
	TelegraphComp->HideShape(AnnularSectorHandle);
	TestFalse(TEXT("Handle hides only its active annular sector"), PooledDecal && PooledDecal->IsVisible());
	TestFalse(TEXT("Released handle can no longer update fill"), TelegraphComp->SetExternalFill(AnnularSectorHandle, 1.0f));

	if (DecalMaterialProperty)
	{
		DecalMaterialProperty->SetObjectPropertyValue_InContainer(TelegraphComp, nullptr);
	}
	TestEqual(TEXT("Missing shared material does not use a fallback"), TelegraphComp->ShowAnnularSector(AnnularSector, AnnularSectorTransform), INDEX_NONE);
	TestFalse(TEXT("Missing shared material leaves the pooled decal hidden"), PooledDecal && PooledDecal->IsVisible());

	TelegraphComp->UnregisterComponent();
	TestWorld->RemoveFromRoot();
	GEngine->DestroyWorldContext(TestWorld);
	TestWorld->DestroyWorld(false);

	return true;
}

#endif
