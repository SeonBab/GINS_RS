#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "RSHitFlashComponent.h"
#include "RSHitFlashUtilities.h"
#include "RSHealthComponent.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSHitFlashAmountTest, "RS.Combat.HitFlash.Amount", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSHitFlashComponentTest, "RS.Combat.HitFlash.Component", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSHitFlashAmountTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Hit flash starts at full brightness"), RSHitFlash::CalculateAmount(0.0f), 1.0f);
	TestEqual(TEXT("Squared ease out reaches quarter brightness halfway"), RSHitFlash::CalculateAmount(0.5f), 0.25f);
	TestEqual(TEXT("Hit flash reaches zero at the end"), RSHitFlash::CalculateAmount(1.0f), 0.0f);
	TestEqual(TEXT("Negative time clamps to the start"), RSHitFlash::CalculateAmount(-1.0f), 1.0f);
	TestEqual(TEXT("Time beyond the duration clamps to the end"), RSHitFlash::CalculateAmount(2.0f), 0.0f);

	return true;
}

bool FRSHitFlashComponentTest::RunTest(const FString& Parameters)
{
	const FName WorldName = MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("RSHitFlashTestWorld"), EUniqueObjectNameOptions::GloballyUnique);
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false, WorldName, GetTransientPackage());
	TestWorld->AddToRoot();
	WorldContext.SetCurrentWorld(TestWorld);
	TestWorld->InitializeActorsForPlay(FURL());

	AActor* Owner = TestWorld->SpawnActor<AActor>();
	URSHealthComponent* HealthComp = NewObject<URSHealthComponent>(Owner);
	USkeletalMeshComponent* MeshComp = NewObject<USkeletalMeshComponent>(Owner);
	URSHitFlashComponent* HitFlashComp = NewObject<URSHitFlashComponent>(Owner);
	Owner->AddInstanceComponent(HealthComp);
	Owner->AddInstanceComponent(MeshComp);
	Owner->AddInstanceComponent(HitFlashComp);

	UMaterialInterface* HitFlashMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Resource/HitFeedback/MI_BossHitFlash.MI_BossHitFlash"));
	FObjectProperty* HitFlashMaterialProperty = FindFProperty<FObjectProperty>(URSHitFlashComponent::StaticClass(), TEXT("HitFlashMaterial"));
	TestNotNull(TEXT("Boss hit flash material instance"), HitFlashMaterial);
	TestNotNull(TEXT("HitFlashMaterial property"), HitFlashMaterialProperty);
	if (!Owner || !HealthComp || !MeshComp || !HitFlashComp || !HitFlashMaterial || !HitFlashMaterialProperty)
	{
		TestWorld->RemoveFromRoot();
		GEngine->DestroyWorldContext(TestWorld);
		TestWorld->DestroyWorld(false);

		return false;
	}

	HitFlashMaterialProperty->SetObjectPropertyValue_InContainer(HitFlashComp, HitFlashMaterial);
	HealthComp->RegisterComponent();
	MeshComp->RegisterComponent();
	HitFlashComp->RegisterComponent();
	HitFlashComp->Initialize(HealthComp, MeshComp);

	UMaterialInterface* PreviousOverlayMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
	MeshComp->SetOverlayMaterial(PreviousOverlayMaterial);

	// HealthComponent 초기 동기화와 회복은 실제 피해가 아니므로 플래시를 시작하지 않습니다
	HealthComp->OnHealthChanged.Broadcast(HealthComp, 100.0f, 100.0f);
	HealthComp->OnHealthChanged.Broadcast(HealthComp, 90.0f, 100.0f);
	TestEqual(TEXT("Equal health and healing preserve the previous overlay"), MeshComp->GetOverlayMaterial(), PreviousOverlayMaterial);
	TestFalse(TEXT("Equal health and healing keep tick disabled"), HitFlashComp->IsComponentTickEnabled());

	HealthComp->OnHealthChanged.Broadcast(HealthComp, 100.0f, 90.0f);
	UMaterialInstanceDynamic* DynamicHitFlashMaterial = Cast<UMaterialInstanceDynamic>(MeshComp->GetOverlayMaterial());
	TestNotNull(TEXT("Health decrease applies a dynamic overlay"), DynamicHitFlashMaterial);
	TestTrue(TEXT("Health decrease enables tick"), HitFlashComp->IsComponentTickEnabled());
	if (DynamicHitFlashMaterial)
	{
		TestEqual(TEXT("Health decrease starts at full brightness"), DynamicHitFlashMaterial->K2_GetScalarParameterValue(TEXT("HitFlashAmount")), 1.0f);

		HitFlashComp->TickComponent(0.06f, LEVELTICK_All, &HitFlashComp->PrimaryComponentTick);
		TestTrue(TEXT("Squared ease out reaches quarter brightness halfway"), FMath::IsNearlyEqual(DynamicHitFlashMaterial->K2_GetScalarParameterValue(TEXT("HitFlashAmount")), 0.25f));

		HealthComp->OnHealthChanged.Broadcast(HealthComp, 90.0f, 95.0f);
		TestTrue(TEXT("Healing does not restart an active flash"), FMath::IsNearlyEqual(DynamicHitFlashMaterial->K2_GetScalarParameterValue(TEXT("HitFlashAmount")), 0.25f));

		HealthComp->OnHealthChanged.Broadcast(HealthComp, 95.0f, 80.0f);
		TestEqual(TEXT("Repeated damage restarts at full brightness"), DynamicHitFlashMaterial->K2_GetScalarParameterValue(TEXT("HitFlashAmount")), 1.0f);
	}

	HitFlashComp->TickComponent(0.12f, LEVELTICK_All, &HitFlashComp->PrimaryComponentTick);
	TestEqual(TEXT("Finishing restores the previous overlay"), MeshComp->GetOverlayMaterial(), PreviousOverlayMaterial);
	TestFalse(TEXT("Finishing disables tick"), HitFlashComp->IsComponentTickEnabled());

	HealthComp->OnHealthChanged.Broadcast(HealthComp, 10.0f, 0.0f);
	TestTrue(TEXT("Final damage also starts the flash"), HitFlashComp->IsComponentTickEnabled());
	HitFlashComp->DestroyComponent();
	TestEqual(TEXT("Destroying restores the previous overlay"), MeshComp->GetOverlayMaterial(), PreviousOverlayMaterial);

	TestWorld->RemoveFromRoot();
	GEngine->DestroyWorldContext(TestWorld);
	TestWorld->DestroyWorld(false);

	return true;
}

#endif
