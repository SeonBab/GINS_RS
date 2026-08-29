// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "RSInputConfig.generated.h"

class UInputAction;

USTRUCT(BlueprintType)
struct FRSInputAction
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<const UInputAction> InputAction = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "InputTag"))
	FGameplayTag InputTag;
};

UCLASS(BlueprintType, Const)
class RS_API URSInputConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	const UInputAction* FindNativeInputAction(const FGameplayTag& InputTag, bool bLogNotFound = true) const;

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TArray<FRSInputAction> NativeInputActions;
};
