// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RSBaseCharacter.h"
#include "RSPlayerCharacter.generated.h"

class UInputMappingContext;
class URSInputConfig;

struct FInputActionValue;

UCLASS()
class RS_API ARSPlayerCharacter : public ARSBaseCharacter
{
	GENERATED_BODY()

public:
	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:
	void Input_Move(const FInputActionValue& InputActionValue);

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<URSInputConfig> InputConfig;
};
