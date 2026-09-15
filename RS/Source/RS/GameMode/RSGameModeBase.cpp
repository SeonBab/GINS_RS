// Fill out your copyright notice in the Description page of Project Settings.


#include "RSGameModeBase.h"

#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "RSBossEncounter.h"
#include "RSPlayerController.h"
#include "RSPlayerHeadUpDisplay.h"
#include "RSPlayerState.h"
#include "RSPlayerCharacter.h"

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
}

void ARSGameModeBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindBossEncounter();

	Super::EndPlay(EndPlayReason);
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
	RequestLocalBossResultPresentation(Result);
}

void ARSGameModeBase::RequestLocalBossResultPresentation(ERSBossEncounterResult Result)
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

		PlayerController->BeginBossResultPresentation(Result);
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
	ExecuteBossResultAction(Action);
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

void ARSGameModeBase::ExecuteBossResultAction(ERSBossResultAction Action)
{
	FName TargetLevelName;
	switch (Action)
	{
	case ERSBossResultAction::RestartLevel:
		TargetLevelName = FName(*UGameplayStatics::GetCurrentLevelName(this, true));
		break;

	case ERSBossResultAction::ReturnToMainMenu:
		TargetLevelName = TEXT("/Game/Maps/MainMenuMap");
		break;

	default:
		return;
	}

	if (TargetLevelName.IsNone())
	{
		BossResultAction.Reset();
		return;
	}

	UGameplayStatics::OpenLevel(this, TargetLevelName, true);
}
