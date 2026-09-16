// Fill out your copyright notice in the Description page of Project Settings.

#include "RSMainMenuGameMode.h"

#include "Kismet/GameplayStatics.h"
#include "RSMainMenuPlayerController.h"
#include "RSMusicPlaybackSubsystem.h"

ARSMainMenuGameMode::ARSMainMenuGameMode()
{
	bStartPlayersAsSpectators = true;
	DefaultPawnClass = nullptr;
	HUDClass = nullptr;
	PlayerControllerClass = ARSMainMenuPlayerController::StaticClass();
	StartLevel = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Maps/Stage.Stage")));
}

void ARSMainMenuGameMode::StartPlay()
{
	Super::StartPlay();
	PlayInitialMusic();
}

bool ARSMainMenuGameMode::RequestStartGame(ARSMainMenuPlayerController* RequestingController)
{
	if (!RequestingController || RequestingController->GetWorld() != GetWorld() || bHasCommittedStartGame || StartLevel.IsNull())
	{
		return false;
	}

	bHasCommittedStartGame = true;

	// 연출을 사용할 수 없으면 기존 동작대로 즉시 전환합니다
	const FSimpleDelegate OnFadedOut = FSimpleDelegate::CreateUObject(this, &ThisClass::OpenStartLevel);
	if (!RequestingController->PlayScreenTransition(OnFadedOut))
	{
		OpenStartLevel();
	}

	return true;
}

void ARSMainMenuGameMode::OpenStartLevel()
{
	UGameplayStatics::OpenLevel(this, StartLevel.ToSoftObjectPath().GetLongPackageFName(), true);
}

void ARSMainMenuGameMode::PlayInitialMusic()
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
