#include "RSLoadingPreparationData.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace
{
	void GetUniqueAssetPaths(const TArray<TSoftObjectPtr<UObject>>& Assets, TArray<FSoftObjectPath>& OutAssetPaths)
	{
		OutAssetPaths.Reset();

		TSet<FSoftObjectPath> UniqueAssetPaths;
		for (const TSoftObjectPtr<UObject>& Asset : Assets)
		{
			const FSoftObjectPath AssetPath = Asset.ToSoftObjectPath();
			if (!AssetPath.IsNull() && !UniqueAssetPaths.Contains(AssetPath))
			{
				UniqueAssetPaths.Add(AssetPath);
				OutAssetPaths.Add(AssetPath);
			}
		}
	}

#if WITH_EDITOR
	EDataValidationResult ValidateAssetList(const TArray<TSoftObjectPtr<UObject>>& Assets, const TCHAR* PropertyName, bool bRequired, FDataValidationContext& Context)
	{
		EDataValidationResult Result = EDataValidationResult::Valid;
		TSet<FSoftObjectPath> UniqueAssetPaths;

		if (bRequired && Assets.IsEmpty())
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("%s must contain at least one asset."), PropertyName)));
			Result = EDataValidationResult::Invalid;
		}

		for (int32 AssetIndex = 0; AssetIndex < Assets.Num(); ++AssetIndex)
		{
			const FSoftObjectPath AssetPath = Assets[AssetIndex].ToSoftObjectPath();
			if (AssetPath.IsNull())
			{
				Context.AddError(FText::FromString(FString::Printf(TEXT("%s[%d] is not configured."), PropertyName, AssetIndex)));
				Result = EDataValidationResult::Invalid;
				continue;
			}

			if (UniqueAssetPaths.Contains(AssetPath))
			{
				Context.AddError(FText::FromString(FString::Printf(TEXT("%s[%d] duplicates %s."), PropertyName, AssetIndex, *AssetPath.ToString())));
				Result = EDataValidationResult::Invalid;
				continue;
			}

			UniqueAssetPaths.Add(AssetPath);
		}

		return Result;
	}
#endif
}

void URSLoadingPreparationData::GetRootAssetPaths(TArray<FSoftObjectPath>& OutAssetPaths) const
{
	GetUniqueAssetPaths(RootAssets, OutAssetPaths);
}

void URSLoadingPreparationData::GetAdditionalAssetPaths(TArray<FSoftObjectPath>& OutAssetPaths) const
{
	GetUniqueAssetPaths(AdditionalAssets, OutAssetPaths);
}

#if WITH_EDITOR
EDataValidationResult URSLoadingPreparationData::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult ValidationResult = Super::IsDataValid(Context);
	ValidationResult = CombineDataValidationResults(ValidationResult, ValidateAssetList(RootAssets, TEXT("RootAssets"), true, Context));
	ValidationResult = CombineDataValidationResults(ValidationResult, ValidateAssetList(AdditionalAssets, TEXT("AdditionalAssets"), false, Context));

	if (!FMath::IsFinite(MaximumRenderingPreparationWaitDuration) || MaximumRenderingPreparationWaitDuration < 1.0f)
	{
		Context.AddError(FText::FromString(TEXT("MaximumRenderingPreparationWaitDuration must be finite and at least 1 second.")));
		ValidationResult = EDataValidationResult::Invalid;
	}

	return ValidationResult;
}
#endif
