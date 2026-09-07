// Fill out your copyright notice in the Description page of Project Settings.


#include "RSGameModeBase.h"

#include "EngineUtils.h"
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
