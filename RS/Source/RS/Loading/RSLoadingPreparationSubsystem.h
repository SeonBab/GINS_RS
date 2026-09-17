#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/ObjectKey.h"
#include "RSLoadingPreparationSubsystem.generated.h"

struct FStreamableHandle;
class URSLoadingPreparationData;

/** 로딩 준비 요청의 현재 상태입니다 */
UENUM(BlueprintType)
enum class ERSLoadingPreparationStatus : uint8
{
	NotRequested,
	LoadingAssets,
	PreparingRendering,
	Ready,
	Failed
};

/** GameInstance 수명에서 에셋 로드와 Niagara PSO 준비를 중복 없이 관리합니다 */
UCLASS()
class RS_API URSLoadingPreparationSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** 진행 중인 비동기 로드와 완료 확인을 정리합니다 */
	virtual void Deinitialize() override;

public:
	/** 준비 묶음을 한 번만 요청하며 이미 요청한 묶음은 기존 작업을 유지합니다 */
	bool RequestPreload(URSLoadingPreparationData* PreparationData);

	/** 준비가 끝났거나 실패하면 Delegate를 실행하며 아직 요청하지 않은 묶음은 함께 요청합니다 */
	bool WaitForPreload(URSLoadingPreparationData* PreparationData, const FSimpleDelegate& CompletionDelegate);

	/** 준비 묶음의 현재 상태를 반환합니다 */
	ERSLoadingPreparationStatus GetPreloadStatus(const URSLoadingPreparationData* PreparationData) const;

	/** 준비 묶음이 보관하는 로드 Handle과 상태를 해제합니다 */
	void ReleasePreload(URSLoadingPreparationData* PreparationData);

#if WITH_DEV_AUTOMATION_TESTS
	/** 자동 테스트에서 루트 의존성으로부터 실제 준비 경로를 수집합니다 */
	static bool BuildPreloadAssetPathsForTest(const URSLoadingPreparationData* PreparationData, TArray<FSoftObjectPath>& OutAssetPaths);
#endif

private:
	/** 루트의 Game 패키지 의존성에서 Niagara System을 찾고 예외 에셋과 합칩니다 */
	static bool BuildPreloadAssetPaths(const URSLoadingPreparationData* PreparationData, TArray<FSoftObjectPath>& OutAssetPaths);

	struct FRSLoadingPreparationRequest
	{
		TWeakObjectPtr<URSLoadingPreparationData> PreparationData;
		ERSLoadingPreparationStatus Status = ERSLoadingPreparationStatus::NotRequested;
		TArray<FSoftObjectPath> AssetPaths;
		TSharedPtr<FStreamableHandle> LoadHandle;
		TArray<FSimpleDelegate> CompletionDelegates;
		double RenderingPreparationStartSeconds = 0.0;
	};

private:
	/** 비동기 에셋 로드가 끝나면 Niagara PSO 준비를 시작합니다 */
	void HandleAssetLoadCompleted(TWeakObjectPtr<URSLoadingPreparationData> PreparationData);

	/** PSO 작업 완료와 제한 시간을 확인합니다 */
	bool TickRenderingPreparation(float DeltaTime);

	/** 요청을 최종 상태로 바꾸고 대기 중인 Delegate를 한 번씩 실행합니다 */
	void FinishRequest(URSLoadingPreparationData* PreparationData, ERSLoadingPreparationStatus FinalStatus);

	/** PSO 완료 확인 Ticker가 필요한 동안 하나만 유지합니다 */
	void EnsureRenderingPreparationTicker();

	/** 더 확인할 요청이 없으면 PSO 완료 확인 Ticker를 제거합니다 */
	void RemoveRenderingPreparationTickerIfIdle();

private:
	/** Data Asset별 비동기 작업과 완료 대기자를 보관합니다 */
	TMap<FObjectKey, FRSLoadingPreparationRequest> Requests;

	/** 준비가 명시적으로 해제되기 전까지 Data Asset의 생명주기를 유지합니다 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<URSLoadingPreparationData>> RetainedPreparationData;

	/** World 전환과 무관하게 PSO 완료를 확인하는 Core Ticker Handle입니다 */
	FTSTicker::FDelegateHandle RenderingPreparationTickerHandle;
};
