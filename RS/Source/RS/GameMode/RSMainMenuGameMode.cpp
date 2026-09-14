// Fill out your copyright notice in the Description page of Project Settings.

#include "RSMainMenuGameMode.h"

#include "Kismet/GameplayStatics.h"
#include "RSMainMenuPlayerController.h"

ARSMainMenuGameMode::ARSMainMenuGameMode()
{
	bStartPlayersAsSpectators = true;
	DefaultPawnClass = nullptr;
	HUDClass = nullptr;
	PlayerControllerClass = ARSMainMenuPlayerController::StaticClass();
	StartLevel = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Maps/Stage.Stage")));
}

bool ARSMainMenuGameMode::RequestStartGame(ARSMainMenuPlayerController* RequestingController)
{
	if (!RequestingController || RequestingController->GetWorld() != GetWorld() || bHasCommittedStartGame || StartLevel.IsNull())
	{
		return false;
	}

	bHasCommittedStartGame = true;
	UGameplayStatics::OpenLevel(this, StartLevel.ToSoftObjectPath().GetLongPackageFName(), true);

	return true;
}
