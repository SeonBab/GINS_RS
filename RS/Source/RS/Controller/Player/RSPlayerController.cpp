// Fill out your copyright notice in the Description page of Project Settings.


#include "RSPlayerController.h"

#include "RSAbilitySystemComponent.h"
#include "RSPlayerState.h"

void ARSPlayerController::PostProcessInput(float DeltaTime, bool bGamePaused)
{
	if (const ARSPlayerState* RSPlayerState = GetPlayerState<ARSPlayerState>())
	{
		if (URSAbilitySystemComponent* AbilitySystemComp = RSPlayerState->GetRSAbilitySystemComponent())
		{
			AbilitySystemComp->ProcessAbilityInput(DeltaTime, bGamePaused);
		}
	}

	Super::PostProcessInput(DeltaTime, bGamePaused);
}
