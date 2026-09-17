#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RSLoadingPreparationData.generated.h"

/** 루트 에셋의 Niagara 의존성을 자동 수집하는 Stage 로딩 준비 프로필입니다 */
UCLASS(BlueprintType, Const)
class RS_API URSLoadingPreparationData : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Niagara 의존성을 탐색할 루트 에셋의 중복과 빈 항목을 제거해 반환합니다 */
	void GetRootAssetPaths(TArray<FSoftObjectPath>& OutAssetPaths) const;

	/** 자동 탐색에서 빠지는 예외 에셋의 중복과 빈 항목을 제거해 반환합니다 */
	void GetAdditionalAssetPaths(TArray<FSoftObjectPath>& OutAssetPaths) const;

	/** PSO 준비가 끝나지 않아도 실패로 전환할 최대 대기 시간을 반환합니다 */
	float GetMaximumRenderingPreparationWaitDuration() const { return MaximumRenderingPreparationWaitDuration; }

#if WITH_DEV_AUTOMATION_TESTS
	/** 자동 테스트에서 루트와 예외 에셋 목록을 구성합니다 */
	void SetAssetSourcesForTest(const TArray<TSoftObjectPtr<UObject>>& InRootAssets, const TArray<TSoftObjectPtr<UObject>>& InAdditionalAssets)
	{
		RootAssets = InRootAssets;
		AdditionalAssets = InAdditionalAssets;
	}
#endif

#if WITH_EDITOR
	/** 빈 참조와 중복 참조를 검사합니다 */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	/** 이 에셋들의 Game 패키지 의존성을 따라가며 Niagara System을 자동 수집합니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Loading", meta = (AllowPrivateAccess = "true"))
	TArray<TSoftObjectPtr<UObject>> RootAssets;

	/** Asset Registry 의존성에 나타나지 않는 동적 참조만 직접 지정합니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Loading", meta = (AllowPrivateAccess = "true"))
	TArray<TSoftObjectPtr<UObject>> AdditionalAssets;

	/** 전역 PSO 작업이 끝나기를 기다릴 최대 시간이며 초과하면 게임 진행을 막지 않고 실패로 완료합니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Loading", meta = (ClampMin = "1.0", UIMin = "1.0", ForceUnits = "s", AllowPrivateAccess = "true"))
	float MaximumRenderingPreparationWaitDuration = 30.0f;
};
