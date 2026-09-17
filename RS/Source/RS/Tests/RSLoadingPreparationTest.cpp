#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "RSLoadingPreparationData.h"
#include "RSLoadingPreparationSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSLoadingPreparationDataTest, "RS.Loading.Preparation.Data", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSLoadingPreparationDataTest::RunTest(const FString& Parameters)
{
	URSLoadingPreparationData* PreparationData = NewObject<URSLoadingPreparationData>();
	const FSoftObjectPath DefaultTexturePath(TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture"));
	const FSoftObjectPath DefaultMaterialPath(TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"));
	PreparationData->SetAssetSourcesForTest(
		{ TSoftObjectPtr<UObject>(), TSoftObjectPtr<UObject>(DefaultTexturePath), TSoftObjectPtr<UObject>(DefaultTexturePath) },
		{ TSoftObjectPtr<UObject>(), TSoftObjectPtr<UObject>(DefaultMaterialPath), TSoftObjectPtr<UObject>(DefaultMaterialPath) });

	TArray<FSoftObjectPath> RootAssetPaths;
	PreparationData->GetRootAssetPaths(RootAssetPaths);
	TestEqual(TEXT("Null and duplicate root assets are removed"), RootAssetPaths.Num(), 1);
	if (RootAssetPaths.Num() == 1)
	{
		TestEqual(TEXT("Root asset order is preserved"), RootAssetPaths[0], DefaultTexturePath);
	}

	TArray<FSoftObjectPath> AdditionalAssetPaths;
	PreparationData->GetAdditionalAssetPaths(AdditionalAssetPaths);
	TestEqual(TEXT("Null and duplicate additional assets are removed"), AdditionalAssetPaths.Num(), 1);
	if (AdditionalAssetPaths.Num() == 1)
	{
		TestEqual(TEXT("Additional asset order is preserved"), AdditionalAssetPaths[0], DefaultMaterialPath);
	}

	TestEqual(TEXT("Default PSO wait duration is thirty seconds"), PreparationData->GetMaximumRenderingPreparationWaitDuration(), 30.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSLoadingPreparationDependencyTest, "RS.Loading.Preparation.Dependencies", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSLoadingPreparationDependencyTest::RunTest(const FString& Parameters)
{
	URSLoadingPreparationData* PreparationData = NewObject<URSLoadingPreparationData>();
	const FSoftObjectPath BossAbilityPath(TEXT("/Game/AbilitySystem/Abilities/Boss/GA_TargetedSlam_1Strike.GA_TargetedSlam_1Strike"));
	const FSoftObjectPath NiagaraPath(TEXT("/Game/Resource/StylizedAOEVfxGroundBurst/VFX/GroundBurst/Niagaras/NS_GroundBurst_Red.NS_GroundBurst_Red"));
	const FSoftObjectPath DefaultTexturePath(TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture"));
	PreparationData->SetAssetSourcesForTest(
		{ TSoftObjectPtr<UObject>(BossAbilityPath) },
		{ TSoftObjectPtr<UObject>(DefaultTexturePath), TSoftObjectPtr<UObject>(NiagaraPath) });

	TArray<FSoftObjectPath> AssetPaths;
	TestTrue(TEXT("Root dependencies can be resolved"), URSLoadingPreparationSubsystem::BuildPreloadAssetPathsForTest(PreparationData, AssetPaths));
	TestEqual(TEXT("A discovered Niagara and an additional asset are prepared once"), AssetPaths.Num(), 2);
	TestTrue(TEXT("Niagara referenced by a boss ability root is discovered through the Asset Registry"), AssetPaths.Contains(NiagaraPath));
	TestTrue(TEXT("Additional asset is preserved"), AssetPaths.Contains(DefaultTexturePath));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSLoadingPreparationEmptyRequestTest, "RS.Loading.Preparation.EmptyRequest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSLoadingPreparationEmptyRequestTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	URSLoadingPreparationSubsystem* LoadingPreparationSubsystem = NewObject<URSLoadingPreparationSubsystem>(GameInstance);
	URSLoadingPreparationData* PreparationData = NewObject<URSLoadingPreparationData>();

	TestEqual(TEXT("Unknown data has not been requested"), LoadingPreparationSubsystem->GetPreloadStatus(PreparationData), ERSLoadingPreparationStatus::NotRequested);
	TestTrue(TEXT("Empty preparation can be requested"), LoadingPreparationSubsystem->RequestPreload(PreparationData));
	TestEqual(TEXT("Empty preparation completes immediately"), LoadingPreparationSubsystem->GetPreloadStatus(PreparationData), ERSLoadingPreparationStatus::Ready);

	bool bCompletionCalled = false;
	const FSimpleDelegate CompletionDelegate = FSimpleDelegate::CreateLambda([&bCompletionCalled]()
	{
		bCompletionCalled = true;
	});
	TestTrue(TEXT("Ready preparation accepts a completion delegate"), LoadingPreparationSubsystem->WaitForPreload(PreparationData, CompletionDelegate));
	TestTrue(TEXT("Ready preparation invokes the delegate immediately"), bCompletionCalled);

	LoadingPreparationSubsystem->ReleasePreload(PreparationData);
	TestEqual(TEXT("Released preparation returns to not requested"), LoadingPreparationSubsystem->GetPreloadStatus(PreparationData), ERSLoadingPreparationStatus::NotRequested);

	return true;
}

#endif
