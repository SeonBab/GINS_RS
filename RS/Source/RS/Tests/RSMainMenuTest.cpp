// Fill out your copyright notice in the Description page of Project Settings.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GameFramework/HUD.h"
#include "RSMainMenuGameMode.h"
#include "RSMainMenuPlayerController.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSMainMenuTest, "RS.UI.MainMenu.Configuration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSMainMenuTest::RunTest(const FString& Parameters)
{
	const ARSMainMenuGameMode* MainMenuGameMode = GetDefault<ARSMainMenuGameMode>();
	TestNotNull(TEXT("Main menu game mode default exists"), MainMenuGameMode);
	if (!MainMenuGameMode)
	{
		return false;
	}

	TestEqual<UClass*>(TEXT("Main menu uses its dedicated player controller"), MainMenuGameMode->PlayerControllerClass.Get(), ARSMainMenuPlayerController::StaticClass());
	TestEqual(TEXT("Start game opens the Stage map"), MainMenuGameMode->GetStartLevel().ToSoftObjectPath().ToString(), FString(TEXT("/Game/Maps/Stage.Stage")));
	TestNull(TEXT("Main menu does not spawn a gameplay pawn"), MainMenuGameMode->DefaultPawnClass.Get());
	TestNull(TEXT("Main menu does not spawn a gameplay HUD"), MainMenuGameMode->HUDClass.Get());

	return true;
}

#endif
