// Fill out your copyright notice in the Description page of Project Settings.


#include "RSPlayerController.h"

#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "Kismet/GameplayStatics.h"
#include "RSAbilitySystemComponent.h"
#include "Combat/RSCombatFunctionLibrary.h"
#include "RSBossEncounter.h"
#include "RSCheatManager.h"
#include "RSGameModeBase.h"
#include "RSGameplayAbility_InteractionScan.h"
#include "RSGameplayTags.h"
#include "RSInGameMenuAction.h"
#include "RSLocalPlayerViewModelSubsystem.h"
#include "RSPlayerCameraComponent.h"
#include "RSPlayerHeadUpDisplay.h"
#include "RSDialogueWidget.h"
#include "RSInGameMenuWidget.h"
#include "RSPlayerState.h"

DEFINE_LOG_CATEGORY_STATIC(LogRSCursorTrace, Log, All);

namespace
{
	// 카메라가 장식용 충돌 안에서 시작해도 실제 커서 대상까지 다시 조회할 수 있을 만큼만 반복합니다
	constexpr int32 RSCursorTraceMaximumOriginBlockers = 8;
	constexpr float RSCursorTraceOriginDistanceTolerance = 1.0f;
}

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
	CloseDialogue();
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

	FVector2D MousePosition = FVector2D::ZeroVector;
	const bool bHasMousePosition = GetMousePosition(MousePosition.X, MousePosition.Y);
	if (!bHasMousePosition)
	{
		return false;
	}

	FHitResult CursorHit;
	FCollisionQueryParams CollisionQueryParams(SCENE_QUERY_STAT(RSCursorWorldLocation), false);
	bool bHasCursorHit = false;
	int32 IgnoredOriginBlockerCount = 0;

	for (int32 TraceAttempt = 0; TraceAttempt <= RSCursorTraceMaximumOriginBlockers; ++TraceAttempt)
	{
		CursorHit = FHitResult();
		bHasCursorHit = GetHitResultAtScreenPosition(MousePosition, ECC_Visibility, CollisionQueryParams, CursorHit);
		if (!bHasCursorHit || !CursorHit.bBlockingHit)
		{
			break;
		}

		const bool bIsOriginBlockingHit = CursorHit.bStartPenetrating || CursorHit.Distance <= RSCursorTraceOriginDistanceTolerance;
		if (!bIsOriginBlockingHit)
		{
			break;
		}

		AActor* OriginBlockingActor = CursorHit.GetActor();
		if (!OriginBlockingActor || TraceAttempt == RSCursorTraceMaximumOriginBlockers)
		{
			break;
		}

		if (URSCombatFunctionLibrary::IsHitCheckDebugEnabled())
		{
			UE_LOG(LogRSCursorTrace, Log,
				TEXT("CursorTrace ignored origin blocker Attempt=%d Actor=%s Component=%s StartPenetrating=%s Distance=%.3f"),
				TraceAttempt + 1,
				*GetNameSafe(OriginBlockingActor),
				*GetNameSafe(CursorHit.GetComponent()),
				CursorHit.bStartPenetrating ? TEXT("true") : TEXT("false"),
				CursorHit.Distance);
		}

		CollisionQueryParams.AddIgnoredActor(OriginBlockingActor);
		++IgnoredOriginBlockerCount;
	}

	if (URSCombatFunctionLibrary::IsHitCheckDebugEnabled())
	{
		FVector MouseWorldOrigin = FVector::ZeroVector;
		FVector MouseWorldDirection = FVector::ZeroVector;
		const bool bHasMouseWorldRay = DeprojectMousePositionToWorld(MouseWorldOrigin, MouseWorldDirection);

		const APawn* ControlledPawn = GetPawn();
		const FVector PawnLocation = ControlledPawn ? ControlledPawn->GetActorLocation() : FVector::ZeroVector;
		const float PawnToImpactDistance2D = ControlledPawn && CursorHit.bBlockingHit
			? FVector::Dist2D(PawnLocation, CursorHit.ImpactPoint)
			: -1.0f;

		UE_LOG(LogRSCursorTrace, Log,
			TEXT("CursorTrace Query=%s Blocking=%s IgnoredOriginBlockers=%d Mouse=%s Screen=(%.1f, %.1f) Deproject=%s RayOrigin=%s RayDirection=%s Pawn=%s PawnLocation=%s HitActor=%s HitComponent=%s ImpactPoint=%s Location=%s HitDistance=%.3f StartPenetrating=%s PawnDistance2D=%.1f TraceStart=%s TraceEnd=%s"),
			bHasCursorHit ? TEXT("true") : TEXT("false"),
			CursorHit.bBlockingHit ? TEXT("true") : TEXT("false"),
			IgnoredOriginBlockerCount,
			bHasMousePosition ? TEXT("true") : TEXT("false"),
			MousePosition.X,
			MousePosition.Y,
			bHasMouseWorldRay ? TEXT("true") : TEXT("false"),
			*MouseWorldOrigin.ToString(),
			*MouseWorldDirection.ToString(),
			*GetNameSafe(ControlledPawn),
			*PawnLocation.ToString(),
			*GetNameSafe(CursorHit.GetActor()),
			*GetNameSafe(CursorHit.GetComponent()),
			*CursorHit.ImpactPoint.ToString(),
			*CursorHit.Location.ToString(),
			CursorHit.Distance,
			CursorHit.bStartPenetrating ? TEXT("true") : TEXT("false"),
			PawnToImpactDistance2D,
			*CursorHit.TraceStart.ToString(),
			*CursorHit.TraceEnd.ToString());
	}

	// 이동 허용 여부는 Character와 Navigation이 판단하고 Controller는 Visibility Hit 위치만 제공합니다
	if (!bHasCursorHit || !CursorHit.bBlockingHit)
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

void ARSPlayerController::BeginBossResultPresentation(ERSBossEncounterResult Result, float RemainingTimeSeconds, int32 HitCount)
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

	if (bIsDialogueOpen && !CloseDialogue())
	{
		return;
	}

	// 기존 입력 정책은 유지하고 HUD에 정적으로 배치된 Result Widget만 표시합니다
	BossResultPresentation.Emplace(Result);
	BossResultRemainingTimeSeconds = FMath::Max(RemainingTimeSeconds, 0.0f);
	BossResultHitCount = FMath::Max(HitCount, 0);

	if (ARSPlayerHeadUpDisplay* PlayerHeadUpDisplay = GetHUD<ARSPlayerHeadUpDisplay>())
	{
		PlayerHeadUpDisplay->ShowBossResultPresentation(Result, BossResultRemainingTimeSeconds, BossResultHitCount);
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
	if (!IsLocalController() || bIsInGameMenuOpen || bIsDialogueOpen || BossResultPresentation.IsSet())
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

bool ARSPlayerController::ToggleDialogue(AActor* SourceActor, const FText& DialogueText)
{
	if (!IsLocalController() || !IsValid(SourceActor) || bIsInGameMenuOpen || BossResultPresentation.IsSet())
	{
		return false;
	}

	if (bIsDialogueOpen)
	{
		if (DialogueSourceActor.Get() == SourceActor)
		{
			return CloseDialogue();
		}

		if (!CloseDialogue())
		{
			return false;
		}
	}

	URSAbilitySystemComponent* AbilitySystemComp = GetRSAbilitySystemComponent();
	ARSPlayerHeadUpDisplay* PlayerHeadUpDisplay = GetHUD<ARSPlayerHeadUpDisplay>();
	URSDialogueWidget* DialogueWidget = PlayerHeadUpDisplay ? PlayerHeadUpDisplay->ShowDialogue(DialogueText) : nullptr;
	if (!AbilitySystemComp || !DialogueWidget)
	{
		if (PlayerHeadUpDisplay)
		{
			PlayerHeadUpDisplay->HideDialogue();
		}

		return false;
	}

	DialogueSourceActor = SourceActor;
	bIsDialogueOpen = true;
	AbilitySystemComp->AddLooseGameplayTag(RSGameplayTags::State_Action_Locked);
	bOwnsDialogueActionLock = true;
	SetInteractionPromptSuppressed(true);
	ConfigureDialogueInput(DialogueWidget);
	DialogueWidget->RequestInitialFocus(this);
	return true;
}

bool ARSPlayerController::CloseDialogue()
{
	if (!IsLocalController() || !bIsDialogueOpen)
	{
		return false;
	}

	if (ARSPlayerHeadUpDisplay* PlayerHeadUpDisplay = GetHUD<ARSPlayerHeadUpDisplay>())
	{
		PlayerHeadUpDisplay->HideDialogue();
	}

	bIsDialogueOpen = false;
	DialogueSourceActor.Reset();

	if (bOwnsDialogueActionLock)
	{
		if (URSAbilitySystemComponent* AbilitySystemComp = GetRSAbilitySystemComponent())
		{
			AbilitySystemComp->RemoveLooseGameplayTag(RSGameplayTags::State_Action_Locked);
		}

		bOwnsDialogueActionLock = false;
	}

	SetInteractionPromptSuppressed(false);
	ConfigureMouseInput();
	return true;
}

void ARSPlayerController::HandleInteractionFocusChanged(AActor* FocusedActor)
{
	if (bIsDialogueOpen && (!DialogueSourceActor.IsValid() || DialogueSourceActor.Get() != FocusedActor))
	{
		CloseDialogue();
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

void ARSPlayerController::ConfigureDialogueInput(URSDialogueWidget* DialogueWidget)
{
	if (!IsLocalController() || !DialogueWidget)
	{
		return;
	}

	bShowMouseCursor = true;

	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetWidgetToFocus(DialogueWidget->TakeWidget());
	SetInputMode(InputMode);
}

void ARSPlayerController::SetInteractionPromptSuppressed(bool bSuppressed)
{
	URSAbilitySystemComponent* AbilitySystemComp = GetRSAbilitySystemComponent();
	if (URSGameplayAbility_InteractionScan* ScanAbility = URSGameplayAbility_InteractionScan::FindInstance(AbilitySystemComp))
	{
		ScanAbility->SetPromptSuppressed(bSuppressed);
	}
}

URSAbilitySystemComponent* ARSPlayerController::GetRSAbilitySystemComponent() const
{
	const ARSPlayerState* RSPlayerState = GetPlayerState<ARSPlayerState>();
	return RSPlayerState ? RSPlayerState->GetRSAbilitySystemComponent() : nullptr;
}

void ARSPlayerController::Input_ToggleInGameMenu()
{
	if (bIsDialogueOpen)
	{
		CloseDialogue();
		return;
	}

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
