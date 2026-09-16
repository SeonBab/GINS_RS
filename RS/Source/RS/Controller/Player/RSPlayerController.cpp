// Fill out your copyright notice in the Description page of Project Settings.


#include "RSPlayerController.h"

#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "Kismet/GameplayStatics.h"
#include "RSAbilitySystemComponent.h"
#include "RSBossEncounter.h"
#include "RSCheatManager.h"
#include "RSGameModeBase.h"
#include "RSInGameMenuAction.h"
#include "RSLocalPlayerViewModelSubsystem.h"
#include "RSPlayerCameraComponent.h"
#include "RSPlayerHeadUpDisplay.h"
#include "RSInGameMenuWidget.h"
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
	bIsInGameMenuOpen = false;
	bWasGamePausedBeforeInGameMenu = false;

	UnregisterViewModelSource(GetPawn());

	Super::EndPlay(EndPlayReason);
}

void ARSPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* EnhancedInputComp = Cast<UEnhancedInputComponent>(InputComponent);
	if (EnhancedInputComp && InGameMenuInputAction)
	{
		EnhancedInputComp->BindAction(InGameMenuInputAction, ETriggerEvent::Started, this, &ThisClass::Input_ToggleInGameMenu);
	}
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

void ARSPlayerController::BeginBossResultPresentation(ERSBossEncounterResult Result)
{
	const bool bIsValidResult = Result == ERSBossEncounterResult::Clear || Result == ERSBossEncounterResult::Failed;
	if (!IsLocalController() || !bIsValidResult || BossResultPresentation.IsSet())
	{
		return;
	}

	if (bIsInGameMenuOpen && !CloseInGameMenu())
	{
		return;
	}

	// 기존 입력 정책은 유지하고 HUD에 정적으로 배치된 Result Widget만 표시합니다
	BossResultPresentation.Emplace(Result);

	if (ARSPlayerHeadUpDisplay* PlayerHeadUpDisplay = GetHUD<ARSPlayerHeadUpDisplay>())
	{
		PlayerHeadUpDisplay->ShowBossResultPresentation(Result);
	}
}

ERSBossEncounterResult ARSPlayerController::GetBossResultPresentation() const
{
	return BossResultPresentation.Get(ERSBossEncounterResult::None);
}

bool ARSPlayerController::RequestBossResultAction(ERSBossResultAction Action)
{
	if (!IsLocalController() || !BossResultPresentation.IsSet())
	{
		return false;
	}

	UWorld* World = GetWorld();
	ARSGameModeBase* GameMode = World ? World->GetAuthGameMode<ARSGameModeBase>() : nullptr;
	return GameMode && GameMode->RequestBossResultAction(this, Action);
}

void ARSPlayerController::HandleBossResultActionAccepted()
{
	if (!IsLocalController())
	{
		return;
	}

	if (ARSPlayerHeadUpDisplay* PlayerHeadUpDisplay = GetHUD<ARSPlayerHeadUpDisplay>())
	{
		PlayerHeadUpDisplay->SetBossResultActionsEnabled(false);
	}
}

bool ARSPlayerController::PlayScreenTransition(const FSimpleDelegate& OnFadedOut)
{
	if (!IsLocalController())
	{
		return false;
	}

	ARSPlayerHeadUpDisplay* PlayerHeadUpDisplay = GetHUD<ARSPlayerHeadUpDisplay>();
	return PlayerHeadUpDisplay && PlayerHeadUpDisplay->PlayScreenTransition(OnFadedOut);
}

bool ARSPlayerController::OpenInGameMenu()
{
	if (!IsLocalController() || bIsInGameMenuOpen || BossResultPresentation.IsSet())
	{
		return false;
	}

	UWorld* World = GetWorld();
	ARSPlayerHeadUpDisplay* PlayerHeadUpDisplay = GetHUD<ARSPlayerHeadUpDisplay>();
	URSInGameMenuWidget* InGameMenuWidget = PlayerHeadUpDisplay ? PlayerHeadUpDisplay->ShowInGameMenu() : nullptr;
	if (!World || !InGameMenuWidget)
	{
		return false;
	}

	bWasGamePausedBeforeInGameMenu = UGameplayStatics::IsGamePaused(World);
	if (!bWasGamePausedBeforeInGameMenu && !UGameplayStatics::SetGamePaused(World, true))
	{
		PlayerHeadUpDisplay->HideInGameMenu();
		return false;
	}

	bIsInGameMenuOpen = true;
	ConfigureInGameMenuInput(InGameMenuWidget);
	InGameMenuWidget->RequestInitialFocus(this);
	return true;
}

bool ARSPlayerController::CloseInGameMenu()
{
	if (!IsLocalController() || !bIsInGameMenuOpen)
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World || (!bWasGamePausedBeforeInGameMenu && !UGameplayStatics::SetGamePaused(World, false)))
	{
		return false;
	}

	if (ARSPlayerHeadUpDisplay* PlayerHeadUpDisplay = GetHUD<ARSPlayerHeadUpDisplay>())
	{
		PlayerHeadUpDisplay->HideInGameMenu();
	}

	bIsInGameMenuOpen = false;
	bWasGamePausedBeforeInGameMenu = false;
	ConfigureMouseInput();
	return true;
}

bool ARSPlayerController::RequestInGameMenuAction(ERSInGameMenuAction Action)
{
	if (!IsLocalController() || !bIsInGameMenuOpen)
	{
		return false;
	}

	UWorld* World = GetWorld();
	ARSGameModeBase* GameMode = World ? World->GetAuthGameMode<ARSGameModeBase>() : nullptr;
	return GameMode && GameMode->RequestInGameMenuAction(this, Action);
}

void ARSPlayerController::HandleInGameMenuActionAccepted()
{
	if (!IsLocalController())
	{
		return;
	}

	if (ARSPlayerHeadUpDisplay* PlayerHeadUpDisplay = GetHUD<ARSPlayerHeadUpDisplay>())
	{
		PlayerHeadUpDisplay->SetInGameMenuActionsEnabled(false);
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

void ARSPlayerController::ConfigureInGameMenuInput(URSInGameMenuWidget* InGameMenuWidget)
{
	if (!IsLocalController() || !InGameMenuWidget)
	{
		return;
	}

	bShowMouseCursor = true;

	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetWidgetToFocus(InGameMenuWidget->TakeWidget());
	SetInputMode(InputMode);
}

void ARSPlayerController::Input_ToggleInGameMenu()
{
	if (bIsInGameMenuOpen)
	{
		CloseInGameMenu();
		return;
	}

	OpenInGameMenu();
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
