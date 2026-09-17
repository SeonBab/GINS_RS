#include "RSLoadingPreparationSubsystem.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Containers/Ticker.h"
#include "Misc/App.h"
#include "Misc/AssetRegistryInterface.h"
#include "NiagaraSystem.h"
#include "RSLoadingPreparationData.h"
#include "ShaderPipelineCache.h"

namespace
{
	constexpr int32 MaxDependencyPackageCount = 10000;

	bool ShouldTraverseDependencyPackage(FName PackageName)
	{
		const FString PackageNameString = PackageName.ToString();
		return !PackageNameString.StartsWith(TEXT("/Engine/")) && !PackageNameString.StartsWith(TEXT("/Script/"));
	}
}

void URSLoadingPreparationSubsystem::Deinitialize()
{
	if (RenderingPreparationTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(RenderingPreparationTickerHandle);
		RenderingPreparationTickerHandle.Reset();
	}

	for (TPair<FObjectKey, FRSLoadingPreparationRequest>& RequestPair : Requests)
	{
		if (RequestPair.Value.LoadHandle.IsValid())
		{
			if (RequestPair.Value.LoadHandle->HasLoadCompleted())
			{
				RequestPair.Value.LoadHandle->ReleaseHandle();
			}
			else
			{
				RequestPair.Value.LoadHandle->CancelHandle();
			}
		}
	}

	Requests.Reset();
	RetainedPreparationData.Reset();

	Super::Deinitialize();
}

bool URSLoadingPreparationSubsystem::RequestPreload(URSLoadingPreparationData* PreparationData)
{
	if (!PreparationData)
	{
		return false;
	}

	const FObjectKey PreparationKey(PreparationData);
	if (Requests.Contains(PreparationKey))
	{
		return true;
	}

	FRSLoadingPreparationRequest& Request = Requests.Add(PreparationKey);
	Request.PreparationData = PreparationData;
	Request.Status = ERSLoadingPreparationStatus::LoadingAssets;
	RetainedPreparationData.AddUnique(PreparationData);

	if (!BuildPreloadAssetPaths(PreparationData, Request.AssetPaths))
	{
		FinishRequest(PreparationData, ERSLoadingPreparationStatus::Failed);
		return false;
	}

	if (Request.AssetPaths.IsEmpty())
	{
		TArray<FSoftObjectPath> RootAssetPaths;
		PreparationData->GetRootAssetPaths(RootAssetPaths);
		if (!RootAssetPaths.IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("Loading preparation %s found no Niagara dependencies or additional assets"), *GetNameSafe(PreparationData));
		}
		FinishRequest(PreparationData, ERSLoadingPreparationStatus::Ready);
		return true;
	}

	const TWeakObjectPtr<URSLoadingPreparationData> WeakPreparationData(PreparationData);
	Request.LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Request.AssetPaths, FStreamableDelegate::CreateUObject(this, &ThisClass::HandleAssetLoadCompleted, WeakPreparationData));
	if (!Request.LoadHandle.IsValid())
	{
		FinishRequest(PreparationData, ERSLoadingPreparationStatus::Failed);
		return false;
	}

	return true;
}

bool URSLoadingPreparationSubsystem::WaitForPreload(URSLoadingPreparationData* PreparationData, const FSimpleDelegate& CompletionDelegate)
{
	if (!PreparationData || !CompletionDelegate.IsBound() || !RequestPreload(PreparationData))
	{
		return false;
	}

	FRSLoadingPreparationRequest* Request = Requests.Find(FObjectKey(PreparationData));
	if (!Request)
	{
		return false;
	}

	if (Request->Status == ERSLoadingPreparationStatus::Ready || Request->Status == ERSLoadingPreparationStatus::Failed)
	{
		CompletionDelegate.Execute();
		return true;
	}

	Request->CompletionDelegates.Add(CompletionDelegate);
	return true;
}

ERSLoadingPreparationStatus URSLoadingPreparationSubsystem::GetPreloadStatus(const URSLoadingPreparationData* PreparationData) const
{
	if (!PreparationData)
	{
		return ERSLoadingPreparationStatus::NotRequested;
	}

	const FRSLoadingPreparationRequest* Request = Requests.Find(FObjectKey(PreparationData));
	return Request ? Request->Status : ERSLoadingPreparationStatus::NotRequested;
}

void URSLoadingPreparationSubsystem::ReleasePreload(URSLoadingPreparationData* PreparationData)
{
	if (!PreparationData)
	{
		return;
	}

	const FObjectKey PreparationKey(PreparationData);
	if (FRSLoadingPreparationRequest* Request = Requests.Find(PreparationKey))
	{
		if (Request->LoadHandle.IsValid())
		{
			if (Request->LoadHandle->HasLoadCompleted())
			{
				Request->LoadHandle->ReleaseHandle();
			}
			else
			{
				Request->LoadHandle->CancelHandle();
			}
		}
	}

	Requests.Remove(PreparationKey);
	RetainedPreparationData.RemoveSingleSwap(PreparationData);
	RemoveRenderingPreparationTickerIfIdle();
}

void URSLoadingPreparationSubsystem::HandleAssetLoadCompleted(TWeakObjectPtr<URSLoadingPreparationData> PreparationData)
{
	URSLoadingPreparationData* LoadedPreparationData = PreparationData.Get();
	FRSLoadingPreparationRequest* Request = LoadedPreparationData ? Requests.Find(FObjectKey(LoadedPreparationData)) : nullptr;
	if (!LoadedPreparationData || !Request || Request->Status != ERSLoadingPreparationStatus::LoadingAssets)
	{
		return;
	}

	for (const FSoftObjectPath& AssetPath : Request->AssetPaths)
	{
		UObject* LoadedAsset = AssetPath.ResolveObject();
		if (!LoadedAsset)
		{
			UE_LOG(LogTemp, Warning, TEXT("Loading preparation %s failed to load %s"), *GetNameSafe(LoadedPreparationData), *AssetPath.ToString());
			FinishRequest(LoadedPreparationData, ERSLoadingPreparationStatus::Failed);
			return;
		}

		if (UNiagaraSystem* NiagaraSystem = Cast<UNiagaraSystem>(LoadedAsset))
		{
			NiagaraSystem->PrecacheAssetPSOs();
		}
	}

	if (!FApp::CanEverRender())
	{
		FinishRequest(LoadedPreparationData, ERSLoadingPreparationStatus::Ready);
		return;
	}

	Request = Requests.Find(FObjectKey(LoadedPreparationData));
	if (!Request)
	{
		return;
	}

	Request->Status = ERSLoadingPreparationStatus::PreparingRendering;
	Request->RenderingPreparationStartSeconds = FPlatformTime::Seconds();
	EnsureRenderingPreparationTicker();
}

bool URSLoadingPreparationSubsystem::BuildPreloadAssetPaths(const URSLoadingPreparationData* PreparationData, TArray<FSoftObjectPath>& OutAssetPaths)
{
	OutAssetPaths.Reset();
	if (!PreparationData)
	{
		return false;
	}

	TSet<FSoftObjectPath> UniqueAssetPaths;
	TArray<FSoftObjectPath> AdditionalAssetPaths;
	PreparationData->GetAdditionalAssetPaths(AdditionalAssetPaths);
	for (const FSoftObjectPath& AdditionalAssetPath : AdditionalAssetPaths)
	{
		if (!UniqueAssetPaths.Contains(AdditionalAssetPath))
		{
			UniqueAssetPaths.Add(AdditionalAssetPath);
			OutAssetPaths.Add(AdditionalAssetPath);
		}
	}

	IAssetRegistry& AssetRegistry = FAssetRegistryModule::GetRegistry();
	TArray<FSoftObjectPath> RootAssetPaths;
	PreparationData->GetRootAssetPaths(RootAssetPaths);

	TArray<FName> PendingPackages;
	TSet<FName> VisitedPackages;
	for (const FSoftObjectPath& RootAssetPath : RootAssetPaths)
	{
		const FName PackageName = RootAssetPath.GetLongPackageFName();
		if (!PackageName.IsNone() && ShouldTraverseDependencyPackage(PackageName))
		{
			PendingPackages.Add(PackageName);
		}
	}

	const FTopLevelAssetPath NiagaraSystemClassPath = UNiagaraSystem::StaticClass()->GetClassPathName();
	while (!PendingPackages.IsEmpty())
	{
		const FName PackageName = PendingPackages.Pop(EAllowShrinking::No);
		if (VisitedPackages.Contains(PackageName))
		{
			continue;
		}

		VisitedPackages.Add(PackageName);
		if (VisitedPackages.Num() > MaxDependencyPackageCount)
		{
			UE_LOG(LogTemp, Error, TEXT("Loading preparation %s exceeded the dependency traversal limit of %d packages"), *GetNameSafe(PreparationData), MaxDependencyPackageCount);
			OutAssetPaths.Reset();
			return false;
		}

		bool bFoundNiagaraSystemInPackage = false;
		TArray<FAssetData> PackageAssets;
		AssetRegistry.GetAssetsByPackageName(PackageName, PackageAssets, true);
		for (const FAssetData& AssetData : PackageAssets)
		{
			if (AssetData.AssetClassPath == NiagaraSystemClassPath)
			{
				const FSoftObjectPath NiagaraAssetPath = AssetData.GetSoftObjectPath();
				if (!UniqueAssetPaths.Contains(NiagaraAssetPath))
				{
					UniqueAssetPaths.Add(NiagaraAssetPath);
					OutAssetPaths.Add(NiagaraAssetPath);
				}
				bFoundNiagaraSystemInPackage = true;
			}
		}

		// Niagara 내부 Material·Texture까지 따라갈 필요는 없으며 System 로드가 자신의 의존성을 불러옵니다
		if (bFoundNiagaraSystemInPackage)
		{
			continue;
		}

		TArray<FName> DependencyPackages;
		AssetRegistry.GetDependencies(
			PackageName,
			DependencyPackages,
			UE::AssetRegistry::EDependencyCategory::Package,
			UE::AssetRegistry::FDependencyQuery(UE::AssetRegistry::EDependencyQuery::Game));

		for (const FName DependencyPackage : DependencyPackages)
		{
			if (!VisitedPackages.Contains(DependencyPackage) && ShouldTraverseDependencyPackage(DependencyPackage))
			{
				PendingPackages.Add(DependencyPackage);
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("Loading preparation %s resolved %d assets from %d dependency packages"), *GetNameSafe(PreparationData), OutAssetPaths.Num(), VisitedPackages.Num());
	return true;
}

#if WITH_DEV_AUTOMATION_TESTS
bool URSLoadingPreparationSubsystem::BuildPreloadAssetPathsForTest(const URSLoadingPreparationData* PreparationData, TArray<FSoftObjectPath>& OutAssetPaths)
{
	return BuildPreloadAssetPaths(PreparationData, OutAssetPaths);
}
#endif

bool URSLoadingPreparationSubsystem::TickRenderingPreparation(float DeltaTime)
{
	TArray<TWeakObjectPtr<URSLoadingPreparationData>> ReadyRequests;
	TArray<TWeakObjectPtr<URSLoadingPreparationData>> FailedRequests;
	const uint32 RemainingPrecompileCount = FShaderPipelineCache::NumPrecompilesRemaining();
	const double CurrentSeconds = FPlatformTime::Seconds();

	for (const TPair<FObjectKey, FRSLoadingPreparationRequest>& RequestPair : Requests)
	{
		const FRSLoadingPreparationRequest& Request = RequestPair.Value;
		URSLoadingPreparationData* PreparationData = Request.PreparationData.Get();
		if (!PreparationData || Request.Status != ERSLoadingPreparationStatus::PreparingRendering)
		{
			continue;
		}

		if (RemainingPrecompileCount == 0)
		{
			ReadyRequests.Add(PreparationData);
			continue;
		}

		const double ElapsedSeconds = CurrentSeconds - Request.RenderingPreparationStartSeconds;
		if (ElapsedSeconds >= PreparationData->GetMaximumRenderingPreparationWaitDuration())
		{
			UE_LOG(LogTemp, Warning, TEXT("Loading preparation %s timed out with %u PSO precompiles remaining"), *GetNameSafe(PreparationData), RemainingPrecompileCount);
			FailedRequests.Add(PreparationData);
		}
	}

	for (const TWeakObjectPtr<URSLoadingPreparationData>& PreparationData : ReadyRequests)
	{
		FinishRequest(PreparationData.Get(), ERSLoadingPreparationStatus::Ready);
	}

	for (const TWeakObjectPtr<URSLoadingPreparationData>& PreparationData : FailedRequests)
	{
		FinishRequest(PreparationData.Get(), ERSLoadingPreparationStatus::Failed);
	}

	for (const TPair<FObjectKey, FRSLoadingPreparationRequest>& RequestPair : Requests)
	{
		if (RequestPair.Value.Status == ERSLoadingPreparationStatus::PreparingRendering)
		{
			return true;
		}
	}

	// false 반환으로 현재 실행 중인 Ticker가 제거되므로 별도 RemoveTicker를 호출하지 않습니다
	RenderingPreparationTickerHandle.Reset();
	return false;
}

void URSLoadingPreparationSubsystem::FinishRequest(URSLoadingPreparationData* PreparationData, ERSLoadingPreparationStatus FinalStatus)
{
	if (!PreparationData)
	{
		return;
	}

	FRSLoadingPreparationRequest* Request = Requests.Find(FObjectKey(PreparationData));
	if (!Request)
	{
		return;
	}

	Request->Status = FinalStatus;
	TArray<FSimpleDelegate> CompletionDelegates = MoveTemp(Request->CompletionDelegates);
	for (const FSimpleDelegate& CompletionDelegate : CompletionDelegates)
	{
		CompletionDelegate.ExecuteIfBound();
	}
}

void URSLoadingPreparationSubsystem::EnsureRenderingPreparationTicker()
{
	if (!RenderingPreparationTickerHandle.IsValid())
	{
		RenderingPreparationTickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::TickRenderingPreparation));
	}
}

void URSLoadingPreparationSubsystem::RemoveRenderingPreparationTickerIfIdle()
{
	bool bHasPendingRenderingPreparation = false;
	for (const TPair<FObjectKey, FRSLoadingPreparationRequest>& RequestPair : Requests)
	{
		if (RequestPair.Value.Status == ERSLoadingPreparationStatus::PreparingRendering)
		{
			bHasPendingRenderingPreparation = true;
			break;
		}
	}

	if (!bHasPendingRenderingPreparation && RenderingPreparationTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(RenderingPreparationTickerHandle);
		RenderingPreparationTickerHandle.Reset();
	}
}
