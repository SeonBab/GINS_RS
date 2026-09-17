// Fill out your copyright notice in the Description page of Project Settings.

#include "RSMainMenuGameMode.h"

#include "Kismet/GameplayStatics.h"
#include "RSLoadingPreparationData.h"
#include "RSLoadingPreparationSubsystem.h"
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
	RequestStartGamePreparation();
}

bool ARSMainMenuGameMode::RequestStartGame(ARSMainMenuPlayerController* RequestingController)
{
	if (!RequestingController || RequestingController->GetWorld() != GetWorld() || bHasCommittedStartGame || StartLevel.IsNull())
	{
		return false;
	}

	bHasCommittedStartGame = true;

	// 연출을 사용할 수 없으면 기존 동작대로 즉시 전환합니다
	const FSimpleDelegate OnFadedOut = FSimpleDelegate::CreateUObject(this, &ThisClass::HandleStartTransitionFadedOut);
	if (!RequestingController->PlayScreenTransition(OnFadedOut))
	{
		HandleStartTransitionFadedOut();
	}

	return true;
}

void ARSMainMenuGameMode::HandleStartTransitionFadedOut()
{
	if (!StartGamePreparation)
	{
		OpenStartLevel();
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	URSLoadingPreparationSubsystem* LoadingPreparationSubsystem = GameInstance ? GameInstance->GetSubsystem<URSLoadingPreparationSubsystem>() : nullptr;
	const FSimpleDelegate OnPreparationFinished = FSimpleDelegate::CreateUObject(this, &ThisClass::OpenStartLevel);
	if (!LoadingPreparationSubsystem || !LoadingPreparationSubsystem->WaitForPreload(StartGamePreparation, OnPreparationFinished))
	{
		// 준비 기반이 없거나 요청에 실패해도 기존 Level 전환을 막지 않습니다
		OpenStartLevel();
	}
}

void ARSMainMenuGameMode::OpenStartLevel()
{
	UGameplayStatics::OpenLevel(this, StartLevel.ToSoftObjectPath().GetLongPackageFName(), true);
}

void ARSMainMenuGameMode::RequestStartGamePreparation()
{
	if (!StartGamePreparation)
	{
		return;
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (URSLoadingPreparationSubsystem* LoadingPreparationSubsystem = GameInstance->GetSubsystem<URSLoadingPreparationSubsystem>())
		{
			LoadingPreparationSubsystem->RequestPreload(StartGamePreparation);
		}
	}
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
