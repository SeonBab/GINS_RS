// Fill out your copyright notice in the Description page of Project Settings.


#include "RSPlayerController.h"

#include "Engine/LocalPlayer.h"
#include "RSAbilitySystemComponent.h"
#include "RSLocalPlayerViewModelSubsystem.h"
#include "RSPlayerCameraComponent.h"
#include "RSPlayerCharacter.h"
#include "RSPlayerState.h"
#include "RSPlayerStatusViewModel.h"

ARSPlayerController::ARSPlayerController()
{
	PlayerCameraComp = CreateDefaultSubobject<URSPlayerCameraComponent>(TEXT("PlayerCameraComponent"));
}

void ARSPlayerController::BeginPlay()
{
	Super::BeginPlay();

	ConfigureMouseInput();

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

bool ARSPlayerController::GetCursorWorldLocation(FVector& OutCursorWorldLocation) const
{
	// 화면 커서가 존재하는 로컬 PlayerController만 월드 위치를 조회할 수 있습니다
	if (!IsLocalController())
	{
		return false;
	}

	FHitResult CursorHit;
	const ETraceTypeQuery TraceChannel = UEngineTypes::ConvertToTraceType(ECC_Visibility);

	// 이동 허용 여부는 Character와 Navigation이 판단하고 Controller는 Visibility Hit 위치만 제공합니다
	if (!GetHitResultUnderCursorByChannel(TraceChannel, false, CursorHit) || !CursorHit.bBlockingHit)
	{
		return false;
	}

	OutCursorWorldLocation = CursorHit.ImpactPoint;

	return true;
}

URSPlayerCameraComponent* ARSPlayerController::GetPlayerCameraComponent() const
{
	return PlayerCameraComp;
}

void ARSPlayerController::ConfigureMouseInput()
{
	if (!IsLocalController())
	{
		return;
	}

	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;

	// HUD가 입력을 먼저 처리할 수 있게 하면서 월드 조작 중에도 마우스 커서를 유지합니다
	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
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
