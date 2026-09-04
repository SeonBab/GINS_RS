// Fill out your copyright notice in the Description page of Project Settings.


#include "RSPlayerController.h"

#include "Engine/LocalPlayer.h"
#include "RSAbilitySystemComponent.h"
#include "RSCheatManager.h"
#include "RSLocalPlayerViewModelSubsystem.h"
#include "RSPlayerCameraComponent.h"
#include "RSPlayerState.h"

ARSPlayerController::ARSPlayerController()
{
	PlayerCameraComp = CreateDefaultSubobject<URSPlayerCameraComponent>(TEXT("PlayerCameraComponent"));

	// 개발용 콘솔 명령은 CheatManager가 소유하며 Shipping 빌드에서는 이 객체가 생성되지 않습니다
	CheatClass = URSCheatManager::StaticClass();
}

void ARSPlayerController::BeginPlay()
{
	Super::BeginPlay();

	ConfigureMouseInput();

	RegisterViewModelSource(GetPawn());
}

void ARSPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterViewModelSource(GetPawn());

	Super::EndPlay(EndPlayReason);
}

void ARSPlayerController::SetPawn(APawn* InPawn)
{
	APawn* PreviousPawn = GetPawn();
	UnregisterViewModelSource(PreviousPawn);

	Super::SetPawn(InPawn);

	RegisterViewModelSource(InPawn);
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

void ARSPlayerController::RegisterViewModelSource(UObject* Source)
{
	if (!IsLocalController() || !IsValid(Source))
	{
		return;
	}

	if (URSLocalPlayerViewModelSubsystem* ViewModelSubsystem = GetViewModelSubsystem())
	{
		ViewModelSubsystem->RegisterSource(Source);
	}
}

void ARSPlayerController::UnregisterViewModelSource(UObject* Source)
{
	if (!IsLocalController() || !Source)
	{
		return;
	}

	if (URSLocalPlayerViewModelSubsystem* ViewModelSubsystem = GetViewModelSubsystem())
	{
		ViewModelSubsystem->UnregisterSource(Source);
	}
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
