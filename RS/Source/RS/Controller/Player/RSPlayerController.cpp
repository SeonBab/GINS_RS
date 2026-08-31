// Fill out your copyright notice in the Description page of Project Settings.


#include "RSPlayerController.h"

#include "Engine/LocalPlayer.h"
#include "RSAbilitySystemComponent.h"
#include "RSLocalPlayerViewModelSubsystem.h"
#include "RSPlayerCharacter.h"
#include "RSPlayerState.h"
#include "RSPlayerStatusViewModel.h"

void ARSPlayerController::BeginPlay()
{
	Super::BeginPlay();

	URSLocalPlayerViewModelSubsystem* ViewModelSubsystem = GetViewModelSubsystem();
	if (!ViewModelSubsystem)
	{
		return;
	}

	ViewModelSubsystem->GetOrCreateViewModel<URSPlayerStatusViewModel>();
	UpdatePlayerStatusViewModelSource();
}

void ARSPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (URSLocalPlayerViewModelSubsystem* ViewModelSubsystem = GetViewModelSubsystem())
	{
		if (URSPlayerStatusViewModel* PlayerStatusViewModel = ViewModelSubsystem->GetViewModel<URSPlayerStatusViewModel>())
		{
			PlayerStatusViewModel->UninitializeViewModel();
		}
	}

	Super::EndPlay(EndPlayReason);
}

void ARSPlayerController::SetPawn(APawn* InPawn)
{
	Super::SetPawn(InPawn);

	UpdatePlayerStatusViewModelSource();
}

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

URSLocalPlayerViewModelSubsystem* ARSPlayerController::GetViewModelSubsystem() const
{
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!LocalPlayer)
	{
		return nullptr;
	}

	return LocalPlayer->GetSubsystem<URSLocalPlayerViewModelSubsystem>();
}

void ARSPlayerController::UpdatePlayerStatusViewModelSource()
{
	URSLocalPlayerViewModelSubsystem* ViewModelSubsystem = GetViewModelSubsystem();
	if (!ViewModelSubsystem)
	{
		return;
	}

	URSPlayerStatusViewModel* PlayerStatusViewModel = ViewModelSubsystem->GetViewModel<URSPlayerStatusViewModel>();
	if (!PlayerStatusViewModel)
	{
		return;
	}

	ARSPlayerCharacter* PlayerCharacter = Cast<ARSPlayerCharacter>(GetPawn());
	if (!IsValid(PlayerCharacter))
	{
		PlayerStatusViewModel->UninitializeViewModel();
		return;
	}

	PlayerStatusViewModel->InitializeViewModel(PlayerCharacter->GetHealthComponent());
}
