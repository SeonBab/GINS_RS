// Fill out your copyright notice in the Description page of Project Settings.


#include "RSGameModeBase.h"

#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "RSBossEncounter.h"
#include "RSInGameMenuAction.h"
#include "RSPlayerController.h"
#include "RSPlayerHeadUpDisplay.h"
#include "RSPlayerState.h"
#include "RSPlayerCharacter.h"
#include "RSMusicPlaybackSubsystem.h"

ARSGameModeBase::ARSGameModeBase()
{
	PlayerStateClass = ARSPlayerState::StaticClass();
	DefaultPawnClass = ARSPlayerCharacter::StaticClass();
	HUDClass = ARSPlayerHeadUpDisplay::StaticClass();
}

void ARSGameModeBase::StartPlay()
{
	BindBossEncounter();

	Super::StartPlay();
	PlayInitialMusic();
}

void ARSGameModeBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindBossEncounter();

	Super::EndPlay(EndPlayReason);
}

void ARSGameModeBase::PlayInitialMusic()
{
	UWorld* World = GetWorld();
	USoundBase* Music = InitialMusic.LoadSynchronous();
	if (!World || !Music)
	{
		return;
	}

	if (URSMusicPlaybackSubsystem* MusicPlaybackSubsystem = World->GetSubsystem<URSMusicPlaybackSubsystem>())
	{
		MusicPlaybackSubsystem->PlayMusic(Music, InitialMusicFadeDuration);
	}
}

void ARSGameModeBase::BindBossEncounter()
{
	UnbindBossEncounter();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<ARSBossEncounter> EncounterIterator(World); EncounterIterator; ++EncounterIterator)
	{
		ARSBossEncounter* BossEncounter = *EncounterIterator;
		if (!IsValid(BossEncounter))
		{
			continue;
		}

		BoundBossEncounter = BossEncounter;
		BossEncounter->OnEncounterFinished.AddUniqueDynamic(this, &ThisClass::HandleBossEncounterFinished);
		return;
	}
}

void ARSGameModeBase::UnbindBossEncounter()
{
	if (ARSBossEncounter* BossEncounter = BoundBossEncounter.Get())
	{
		BossEncounter->OnEncounterFinished.RemoveDynamic(this, &ThisClass::HandleBossEncounterFinished);
	}

	BoundBossEncounter.Reset();
}

void ARSGameModeBase::HandleBossEncounterFinished(ARSBossEncounter* BossEncounter, ERSBossEncounterResult Result)
{
	const bool bIsValidResult = Result == ERSBossEncounterResult::Clear || Result == ERSBossEncounterResult::Failed;
	if (BossEncounter != BoundBossEncounter.Get() || !bIsValidResult || BossResult.IsSet())
	{
		return;
	}

	BossResult.Emplace(Result);
	RequestLocalBossResultPresentation(Result, BossEncounter->GetRemainingTimeSeconds(), BossEncounter->GetHitCount());
}

void ARSGameModeBase::RequestLocalBossResultPresentation(ERSBossEncounterResult Result, float RemainingTimeSeconds, int32 HitCount)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<ARSPlayerController> PlayerControllerIterator(World); PlayerControllerIterator; ++PlayerControllerIterator)
	{
		ARSPlayerController* PlayerController = *PlayerControllerIterator;
		if (!IsValid(PlayerController) || !PlayerController->IsLocalController())
		{
			continue;
		}

		PlayerController->BeginBossResultPresentation(Result, RemainingTimeSeconds, HitCount);
		return;
	}
}

bool ARSGameModeBase::RequestBossResultAction(ARSPlayerController* RequestingPlayerController, ERSBossResultAction Action)
{
	if (!IsValid(RequestingPlayerController) || RequestingPlayerController->GetWorld() != GetWorld() || !RequestingPlayerController->IsLocalController())
	{
		return false;
	}

	if (!BossResult.IsSet() || RequestingPlayerController->GetBossResultPresentation() != BossResult.GetValue())
	{
		return false;
	}

	if (!TryCommitBossResultAction(Action))
	{
		return false;
	}

	// Local Presentation의 재입력을 먼저 막은 뒤 World 전환을 실행합니다
	RequestingPlayerController->HandleBossResultActionAccepted();
	ExecuteBossResultAction(Action, RequestingPlayerController);
	return true;
}

bool ARSGameModeBase::TryCommitBossResultAction(ERSBossResultAction Action)
{
	const ERSBossEncounterResult CurrentResult = BossResult.Get(ERSBossEncounterResult::None);
	const bool bIsValidResult = CurrentResult == ERSBossEncounterResult::Clear || CurrentResult == ERSBossEncounterResult::Failed;
	const bool bIsValidAction = Action == ERSBossResultAction::RestartLevel || Action == ERSBossResultAction::ReturnToMainMenu;
	if (!bIsValidResult || !bIsValidAction || BossResultAction.IsSet())
	{
		return false;
	}

	BossResultAction.Emplace(Action);
	return true;
}

void ARSGameModeBase::ExecuteBossResultAction(ERSBossResultAction Action, ARSPlayerController* RequestingPlayerController)
{
	const bool bIsValidAction = Action == ERSBossResultAction::RestartLevel || Action == ERSBossResultAction::ReturnToMainMenu;
	if (!bIsValidAction || !ExecuteLevelTransition(Action == ERSBossResultAction::RestartLevel, RequestingPlayerController))
	{
		BossResultAction.Reset();
	}
}

bool ARSGameModeBase::RequestInGameMenuAction(ARSPlayerController* RequestingPlayerController, ERSInGameMenuAction Action)
{
	if (!IsValid(RequestingPlayerController) || RequestingPlayerController->GetWorld() != GetWorld() || !RequestingPlayerController->IsLocalController())
	{
		return false;
	}

	if (!RequestingPlayerController->IsInGameMenuOpen() || BossResult.IsSet() || !TryCommitInGameMenuAction(Action))
	{
		return false;
	}

	RequestingPlayerController->HandleInGameMenuActionAccepted();
	ExecuteInGameMenuAction(Action, RequestingPlayerController);
	return true;
}

bool ARSGameModeBase::TryCommitInGameMenuAction(ERSInGameMenuAction Action)
{
	const bool bIsValidAction = Action == ERSInGameMenuAction::RestartLevel || Action == ERSInGameMenuAction::ReturnToMainMenu;
	if (!bIsValidAction || InGameMenuAction.IsSet() || BossResultAction.IsSet())
	{
		return false;
	}

	InGameMenuAction.Emplace(Action);
	return true;
}

void ARSGameModeBase::ExecuteInGameMenuAction(ERSInGameMenuAction Action, ARSPlayerController* RequestingPlayerController)
{
	const bool bIsValidAction = Action == ERSInGameMenuAction::RestartLevel || Action == ERSInGameMenuAction::ReturnToMainMenu;
	if (!bIsValidAction || !ExecuteLevelTransition(Action == ERSInGameMenuAction::RestartLevel, RequestingPlayerController))
	{
		InGameMenuAction.Reset();
	}
}

bool ARSGameModeBase::ExecuteLevelTransition(bool bRestartCurrentLevel, ARSPlayerController* RequestingPlayerController)
{
	const FName TargetLevelName = bRestartCurrentLevel
		? FName(*UGameplayStatics::GetCurrentLevelName(this, true))
		: FName(TEXT("/Game/Maps/MainMenuMap"));
	if (TargetLevelName.IsNone())
	{
		return false;
	}

	// 연출을 사용할 수 없으면 기존 동작대로 즉시 전환합니다
	const FSimpleDelegate OnFadedOut = FSimpleDelegate::CreateUObject(this, &ThisClass::OpenTransitionLevel, TargetLevelName);
	if (!RequestingPlayerController || !RequestingPlayerController->PlayScreenTransition(OnFadedOut))
	{
		OpenTransitionLevel(TargetLevelName);
	}

	return true;
}

void ARSGameModeBase::OpenTransitionLevel(FName TargetLevelName)
{
	UGameplayStatics::OpenLevel(this, TargetLevelName, true);
}
