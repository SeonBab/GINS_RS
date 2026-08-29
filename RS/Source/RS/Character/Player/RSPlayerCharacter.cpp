// Fill out your copyright notice in the Description page of Project Settings.


#include "RSPlayerCharacter.h"

#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "RSGameplayTags.h"
#include "RSInputComponent.h"
#include "RSInputConfig.h"

// Called to bind functionality to input
void ARSPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	APlayerController* PlayerController = Cast<APlayerController>(GetController());

	if (!PlayerController)
	{
		return;
	}

	ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer();

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
	{
		if (DefaultMappingContext)
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}

	URSInputComponent* RSInputComponent = CastChecked<URSInputComponent>(PlayerInputComponent);

	if (!InputConfig)
	{
		UE_LOG(LogTemp, Error, TEXT("%s has no InputConfig"), *GetNameSafe(this));
		return;
	}

	RSInputComponent->BindNativeAction(InputConfig, RSGameplayTags::InputTag_Move, ETriggerEvent::Triggered, this, &ThisClass::Input_Move);

}

void ARSPlayerCharacter::Input_Move(const FInputActionValue& InputActionValue)
{
	if (!Controller)
	{
		return;
	}

	const FVector2D MovementValue = InputActionValue.Get<FVector2D>();

	const FRotator ControlRotation = Controller->GetControlRotation();
	const FRotator YawRotation(0.0f, ControlRotation.Yaw, 0.0f);

	const FVector ForwardDirection = YawRotation.RotateVector(FVector::ForwardVector);
	const FVector RightDirection = YawRotation.RotateVector(FVector::RightVector);

	AddMovementInput(ForwardDirection, MovementValue.Y);
	AddMovementInput(RightDirection, MovementValue.X);
}
