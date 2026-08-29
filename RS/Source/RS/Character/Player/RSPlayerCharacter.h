// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RSBaseCharacter.h"
#include "RSPlayerCharacter.generated.h"

class UInputMappingContext;
class URSInputConfig;
class UCameraComponent;
class USpringArmComponent;

struct FInputActionValue;

UCLASS()
class RS_API ARSPlayerCharacter : public ARSBaseCharacter
{
	GENERATED_BODY()

public:
	ARSPlayerCharacter();

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:
	void Input_Move(const FInputActionValue& InputActionValue);

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<URSInputConfig> InputConfig;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Components|Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> CameraComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Components|Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> SpringArmComp;
};
