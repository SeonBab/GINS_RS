// Fill out your copyright notice in the Description page of Project Settings.

#include "RSPlayerHeadUpDisplay.h"

#include "Engine/LocalPlayer.h"
#include "MVVMSubsystem.h"
#include "MVVMViewModelBase.h"
#include "Components/PanelWidget.h"
#include "RSBossEncounter.h"
#include "RSBossResultWidget.h"
#include "RSDialogueWidget.h"
#include "RSInGameMenuAction.h"
#include "RSInGameMenuWidget.h"
#include "RSLocalPlayerViewModelSubsystem.h"
#include "RSMusicPlaybackSubsystem.h"
#include "RSPlayerController.h"
#include "RSPrimaryLayout.h"
#include "RSScreenFadeWidget.h"
#include "View/MVVMView.h"
#include "View/MVVMViewClass.h"

void ARSPlayerHeadUpDisplay::BeginPlay()
{
	Super::BeginPlay();

	CreatePrimaryLayout();
}

void ARSPlayerHeadUpDisplay::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (URSInGameMenuWidget* InGameMenuWidget = FindInGameMenuWidget())
	{
		InGameMenuWidget->GetContinueRequested().RemoveAll(this);
		InGameMenuWidget->GetRestartRequested().RemoveAll(this);
		InGameMenuWidget->GetMainMenuRequested().RemoveAll(this);
	}

	if (URSBossResultWidget* BossResultWidget = FindBossResultWidget())
	{
		BossResultWidget->GetBossResultActionRequested().RemoveAll(this);
	}

	if (DialogueWidget)
	{
		DialogueWidget->GetCloseRequested().RemoveAll(this);
	}

	if (PrimaryLayout)
	{
		PrimaryLayout->RemoveFromParent();
		PrimaryLayout = nullptr;
	}

	DialogueWidget = nullptr;

	Super::EndPlay(EndPlayReason);
}

URSPrimaryLayout* ARSPlayerHeadUpDisplay::GetPrimaryLayout() const
{
	return PrimaryLayout;
}

void ARSPlayerHeadUpDisplay::ShowBossResultPresentation(ERSBossEncounterResult Result, float RemainingTimeSeconds, int32 HitCount)
{
	const bool bIsValidResult = Result == ERSBossEncounterResult::Clear || Result == ERSBossEncounterResult::Failed;
	URSBossResultWidget* BossResultWidget = FindBossResultWidget();
	if (!bIsValidResult || !BossResultWidget)
	{
		return;
	}

	BossResultWidget->GetBossResultActionRequested().RemoveAll(this);
	BossResultWidget->GetBossResultActionRequested().AddUObject(this, &ThisClass::HandleBossResultActionRequested);
	BossResultWidget->SetActionsEnabled(true);
	BossResultWidget->PresentResult(Result, RemainingTimeSeconds, HitCount);

	// Widget 자신은 뒤쪽 입력을 막지 않고 Action Button만 Pointer 입력을 받게 합니다
	BossResultWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void ARSPlayerHeadUpDisplay::SetBossResultActionsEnabled(bool bEnabled)
{
	if (URSBossResultWidget* BossResultWidget = FindBossResultWidget())
	{
		BossResultWidget->SetActionsEnabled(bEnabled);
	}
}

URSInGameMenuWidget* ARSPlayerHeadUpDisplay::ShowInGameMenu()
{
	URSInGameMenuWidget* InGameMenuWidget = FindInGameMenuWidget();
	if (!InGameMenuWidget || !InGameMenuWidget->OpenMenu())
	{
		return nullptr;
	}

	InGameMenuWidget->SetActionsEnabled(true);
	InGameMenuWidget->SetVisibility(ESlateVisibility::Visible);
	return InGameMenuWidget;
}

void ARSPlayerHeadUpDisplay::HideInGameMenu()
{
	if (URSInGameMenuWidget* InGameMenuWidget = FindInGameMenuWidget())
	{
		InGameMenuWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void ARSPlayerHeadUpDisplay::SetInGameMenuActionsEnabled(bool bEnabled)
{
	if (URSInGameMenuWidget* InGameMenuWidget = FindInGameMenuWidget())
	{
		InGameMenuWidget->SetActionsEnabled(bEnabled);
	}
}

URSDialogueWidget* ARSPlayerHeadUpDisplay::ShowDialogue(const FText& DialogueText)
{
	if (!DialogueWidget || !DialogueWidget->ShowDialogue(DialogueText))
	{
		return nullptr;
	}

	return DialogueWidget;
}

void ARSPlayerHeadUpDisplay::HideDialogue()
{
	if (DialogueWidget)
	{
		DialogueWidget->HideDialogue();
	}
}

bool ARSPlayerHeadUpDisplay::PlayScreenTransition(const FSimpleDelegate& OnFadedOut)
{
	URSScreenFadeWidget* ScreenFadeWidget = FindScreenFadeWidget();
	if (!ScreenFadeWidget || !ScreenFadeWidget->PlayFadeOutThen(OnFadedOut))
	{
		return false;
	}

	// 화면과 음악이 같은 길이로 사라지게 합니다
	if (UWorld* World = GetWorld())
	{
		if (URSMusicPlaybackSubsystem* MusicPlaybackSubsystem = World->GetSubsystem<URSMusicPlaybackSubsystem>())
		{
			MusicPlaybackSubsystem->StopMusic(ScreenFadeWidget->GetFadeOutDuration());
		}
	}

	return true;
}

void ARSPlayerHeadUpDisplay::CreatePrimaryLayout()
{
	APlayerController* OwningPlayerController = GetOwningPlayerController();
	if (!OwningPlayerController || !OwningPlayerController->IsLocalController() || !PrimaryLayoutClass)
	{
		return;
	}

	PrimaryLayout = CreateWidget<URSPrimaryLayout>(OwningPlayerController, PrimaryLayoutClass);
	if (!PrimaryLayout)
	{
		return;
	}

	SetSharedViewModels();
	InitializeInGameMenu();
	InitializeDialogue();
	PrimaryLayout->AddToViewport();

	// Level에 진입할 때마다 검정 화면에서 시작합니다
	if (URSScreenFadeWidget* ScreenFadeWidget = FindScreenFadeWidget())
	{
		ScreenFadeWidget->PlayFadeIn();
	}

	// HUD보다 Result Flow가 먼저 시작된 경우에도 현재 Local Presentation을 복원합니다
	const ARSPlayerController* PlayerController = Cast<ARSPlayerController>(OwningPlayerController);
	if (PlayerController && PlayerController->HasBossResultPresentationStarted())
	{
		ShowBossResultPresentation(
			PlayerController->GetBossResultPresentation(),
			PlayerController->GetBossResultRemainingTimeSeconds(),
			PlayerController->GetBossResultHitCount());
	}
}

void ARSPlayerHeadUpDisplay::SetSharedViewModels()
{
	if (!PrimaryLayout)
	{
		return;
	}

	APlayerController* PlayerController = GetOwningPlayerController();
	ULocalPlayer* LocalPlayer = PlayerController ? PlayerController->GetLocalPlayer() : nullptr;
	if (!LocalPlayer)
	{
		return;
	}

	URSLocalPlayerViewModelSubsystem* ViewModelSubsystem = LocalPlayer->GetSubsystem<URSLocalPlayerViewModelSubsystem>();
	UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(PrimaryLayout);
	if (!ViewModelSubsystem || !View || !View->GetViewClass())
	{
		return;
	}

	for (const FMVVMViewClass_Source& Source : View->GetViewClass()->GetSources())
	{
		if (!Source.IsViewModel() || !Source.CanBeSet())
		{
			continue;
		}

		UClass* ViewModelClass = Source.GetSourceClass();
		if (!ViewModelClass || !ViewModelClass->IsChildOf(UMVVMViewModelBase::StaticClass()))
		{
			continue;
		}

		UMVVMViewModelBase* ViewModel = ViewModelSubsystem->GetOrCreateViewModelByClass(ViewModelClass);
		if (ViewModel)
		{
			PrimaryLayout->SetViewModel(ViewModel);
		}
	}
}

URSBossResultWidget* ARSPlayerHeadUpDisplay::FindBossResultWidget() const
{
	UPanelWidget* MenuLayer = PrimaryLayout ? PrimaryLayout->GetLayer(ERSWidgetLayer::Menu) : nullptr;
	if (!MenuLayer)
	{
		return nullptr;
	}

	for (int32 ChildIndex = 0; ChildIndex < MenuLayer->GetChildrenCount(); ++ChildIndex)
	{
		if (URSBossResultWidget* BossResultWidget = Cast<URSBossResultWidget>(MenuLayer->GetChildAt(ChildIndex)))
		{
			return BossResultWidget;
		}
	}

	return nullptr;
}

URSScreenFadeWidget* ARSPlayerHeadUpDisplay::FindScreenFadeWidget() const
{
	UPanelWidget* TransitionLayer = PrimaryLayout ? PrimaryLayout->GetLayer(ERSWidgetLayer::Transition) : nullptr;
	if (!TransitionLayer)
	{
		return nullptr;
	}

	for (int32 ChildIndex = 0; ChildIndex < TransitionLayer->GetChildrenCount(); ++ChildIndex)
	{
		if (URSScreenFadeWidget* ScreenFadeWidget = Cast<URSScreenFadeWidget>(TransitionLayer->GetChildAt(ChildIndex)))
		{
			return ScreenFadeWidget;
		}
	}

	return nullptr;
}

URSInGameMenuWidget* ARSPlayerHeadUpDisplay::FindInGameMenuWidget() const
{
	UPanelWidget* MenuLayer = PrimaryLayout ? PrimaryLayout->GetLayer(ERSWidgetLayer::Menu) : nullptr;
	if (!MenuLayer)
	{
		return nullptr;
	}

	for (int32 ChildIndex = 0; ChildIndex < MenuLayer->GetChildrenCount(); ++ChildIndex)
	{
		if (URSInGameMenuWidget* InGameMenuWidget = Cast<URSInGameMenuWidget>(MenuLayer->GetChildAt(ChildIndex)))
		{
			return InGameMenuWidget;
		}
	}

	return nullptr;
}

URSDialogueWidget* ARSPlayerHeadUpDisplay::FindDialogueWidget() const
{
	UPanelWidget* ModalLayer = PrimaryLayout ? PrimaryLayout->GetLayer(ERSWidgetLayer::Modal) : nullptr;
	if (!ModalLayer)
	{
		return nullptr;
	}

	for (int32 ChildIndex = 0; ChildIndex < ModalLayer->GetChildrenCount(); ++ChildIndex)
	{
		if (URSDialogueWidget* FoundDialogueWidget = Cast<URSDialogueWidget>(ModalLayer->GetChildAt(ChildIndex)))
		{
			return FoundDialogueWidget;
		}
	}

	return nullptr;
}

void ARSPlayerHeadUpDisplay::InitializeInGameMenu()
{
	if (URSInGameMenuWidget* InGameMenuWidget = FindInGameMenuWidget())
	{
		InGameMenuWidget->GetContinueRequested().RemoveAll(this);
		InGameMenuWidget->GetContinueRequested().AddUObject(this, &ThisClass::HandleInGameMenuContinueRequested);
		InGameMenuWidget->GetRestartRequested().RemoveAll(this);
		InGameMenuWidget->GetRestartRequested().AddUObject(this, &ThisClass::HandleInGameMenuActionRequested, ERSInGameMenuAction::RestartLevel);
		InGameMenuWidget->GetMainMenuRequested().RemoveAll(this);
		InGameMenuWidget->GetMainMenuRequested().AddUObject(this, &ThisClass::HandleInGameMenuActionRequested, ERSInGameMenuAction::ReturnToMainMenu);
		InGameMenuWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void ARSPlayerHeadUpDisplay::InitializeDialogue()
{
	DialogueWidget = FindDialogueWidget();
	if (!DialogueWidget)
	{
		return;
	}

	DialogueWidget->GetCloseRequested().RemoveAll(this);
	DialogueWidget->GetCloseRequested().AddUObject(this, &ThisClass::HandleDialogueCloseRequested);
	DialogueWidget->HideDialogue();
}

void ARSPlayerHeadUpDisplay::HandleBossResultActionRequested(ERSBossResultAction Action)
{
	if (ARSPlayerController* PlayerController = Cast<ARSPlayerController>(GetOwningPlayerController()))
	{
		PlayerController->RequestBossResultAction(Action);
	}
}

void ARSPlayerHeadUpDisplay::HandleInGameMenuContinueRequested()
{
	if (ARSPlayerController* PlayerController = Cast<ARSPlayerController>(GetOwningPlayerController()))
	{
		PlayerController->CloseInGameMenu();
	}
}

void ARSPlayerHeadUpDisplay::HandleInGameMenuActionRequested(ERSInGameMenuAction Action)
{
	if (ARSPlayerController* PlayerController = Cast<ARSPlayerController>(GetOwningPlayerController()))
	{
		PlayerController->RequestInGameMenuAction(Action);
	}
}

void ARSPlayerHeadUpDisplay::HandleDialogueCloseRequested()
{
	if (ARSPlayerController* PlayerController = Cast<ARSPlayerController>(GetOwningPlayerController()))
	{
		PlayerController->CloseDialogue();
	}
}
