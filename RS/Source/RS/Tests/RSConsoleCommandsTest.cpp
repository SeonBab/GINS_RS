#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Misc/AutomationTest.h"
#include "HAL/IConsoleManager.h"
#include "RSBossPhaseComponent.h"
#include "RSBossPhaseData.h"
#include "RSGameplayAbility_ArmSwing.h"
#include "RSGameplayAbility_ConcentricRings.h"
#include "RSGameplayAbility_TargetedSlam.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "RSCheatManager.h"
#include "RSPlayerState.h"
#include "RSAbilitySystemComponent.h"
#include "RSHealthComponent.h"
#include "RSHealthSet.h"
#include "RSGameplayTags.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSConsolePlayerHealthTest, "RS.Debug.ConsoleCommands.PlayerHealth", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSConsolePlayerHealthTest::RunTest(const FString& Parameters)
{
	const FName Name = MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("RSConsoleTestWorld"), EUniqueObjectNameOptions::GloballyUnique);
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, Name, GetTransientPackage());
	World->AddToRoot();
	Context.SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	World->BeginPlay();
	APlayerController* Controller = World->SpawnActor<APlayerController>();
	ARSPlayerState* PlayerState = World->SpawnActor<ARSPlayerState>();
	AActor* Avatar = World->SpawnActor<AActor>();
	Controller->SetPlayerState(PlayerState);
	URSAbilitySystemComponent* AbilitySystemComp = PlayerState->GetRSAbilitySystemComponent();
	AbilitySystemComp->InitAbilityActorInfo(PlayerState, Avatar);
	URSHealthComponent* Health = NewObject<URSHealthComponent>(Avatar);
	Health->RegisterComponent();
	Health->InitializeWithAbilitySystem(AbilitySystemComp);
	AbilitySystemComp->SetNumericAttributeBase(URSHealthSet::GetMaxHealthAttribute(), 100.0f);
	AbilitySystemComp->SetNumericAttributeBase(URSHealthSet::GetHealthAttribute(), 100.0f);
	URSCheatManager* Cheat = NewObject<URSCheatManager>(Controller);
	Cheat->DamagePlayer(30);
	TestEqual(TEXT("Damage uses actual GameplayEffect"), Health->GetHealth(), 70.0f);
	Cheat->HealPlayer(10);
	TestEqual(TEXT("Healing uses actual GameplayEffect"), Health->GetHealth(), 80.0f);
	Cheat->HealPlayer(1000);
	TestEqual(TEXT("Healing clamps to maximum"), Health->GetHealth(), 100.0f);
	Cheat->SetPlayerInvulnerable(true);
	Cheat->SetPlayerInvulnerable(true);
	Cheat->DamagePlayer(30);
	TestEqual(TEXT("Cheat immunity blocks damage"), Health->GetHealth(), 100.0f);
	AbilitySystemComp->AddLooseGameplayTag(RSGameplayTags::State_Immunity_Damage);
	Cheat->SetPlayerInvulnerable(false);
	TestTrue(TEXT("Disabling preserves another immunity source"), AbilitySystemComp->HasMatchingGameplayTag(RSGameplayTags::State_Immunity_Damage));
	AbilitySystemComp->RemoveLooseGameplayTag(RSGameplayTags::State_Immunity_Damage);
	TestFalse(TEXT("Repeated enable did not leak an extra count"), AbilitySystemComp->HasMatchingGameplayTag(RSGameplayTags::State_Immunity_Damage));
	Cheat->SetPlayerInvulnerable(true);
	Cheat->KillPlayer();
	TestTrue(TEXT("Kill invokes HealthComponent death despite immunity"), Health->IsDead());
	TestTrue(TEXT("Death pipeline adds its tag"), AbilitySystemComp->HasMatchingGameplayTag(RSGameplayTags::State_Dead));
	Cheat->HealPlayer(100);
	TestEqual(TEXT("Heal cannot resurrect"), Health->GetHealth(), 0.0f);
	Cheat->SetPlayerInvulnerable(false);
	Health->UninitializeFromAbilitySystem();
	World->RemoveFromRoot();
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSConsoleCommandsRegistrationTest, "RS.Debug.ConsoleCommands.Registration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSConsoleCommandsRegistrationTest::RunTest(const FString& Parameters)
{
	for (const TCHAR* Name : { TEXT("RS.Player.Damage"), TEXT("RS.Player.Heal"), TEXT("RS.Player.Kill"), TEXT("RS.Player.Invulnerable"), TEXT("RS.Debug.DumpState"), TEXT("RS.Boss.RunPattern"), TEXT("RS.Player.ToggleDownedTag"), TEXT("RS.Boss.Damage"), TEXT("RS.Boss.CompleteEncounter"), TEXT("RS.Boss.ExpireTimeLimit") })
	{
		IConsoleObject* Object = IConsoleManager::Get().FindConsoleObject(Name);
		TestNotNull(Name, Object);
		if (Object)
		{
			TestTrue(TEXT("Registered as a cheat command"), Object->AsCommand() && Object->TestFlags(ECVF_Cheat));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSConsolePatternQueueTest, "RS.Debug.ConsoleCommands.PatternQueue", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSConsolePatternQueueTest::RunTest(const FString& Parameters)
{
	URSBossPhaseComponent* Phase = NewObject<URSBossPhaseComponent>();
	FRSBossPhaseDefinition Definition;
	Definition.BasicPatterns = { URSGameplayAbility_ArmSwing::StaticClass(), URSGameplayAbility_TargetedSlam::StaticClass() };
	Definition.SpecialPatterns = { URSGameplayAbility_ConcentricRings::StaticClass() };
	Phase->SetPhasesForTest({ Definition, Definition });
	TSubclassOf<URSBaseGameplayAbility> Selected;
	const FString ArmSwingName = URSGameplayAbility_ArmSwing::StaticClass()->GetName();
	const FString SlamName = URSGameplayAbility_TargetedSlam::StaticClass()->GetName();
	TestTrue(TEXT("Queue first basic"), Phase->QueueDebugPattern(ArmSwingName));
	TestTrue(TEXT("Queue does not change the cycle"), Phase->GetCurrentCycleIndex() == 0);
	TestTrue(TEXT("Different type may still select normally"), Phase->TrySelectPattern(ERSBossPatternType::Special, Selected));
	TestTrue(TEXT("Queued type selects"), Phase->TrySelectPattern(ERSBossPatternType::Basic, Selected));
	TestTrue(TEXT("Reservation survives another pattern type"), Selected == URSGameplayAbility_ArmSwing::StaticClass());
	Phase->TrySelectPattern(ERSBossPatternType::Basic, Selected);
	TestTrue(TEXT("Reservation consumed once; no-repeat policy restored"), Selected == URSGameplayAbility_TargetedSlam::StaticClass());

	Phase->QueueDebugPattern(SlamName);
	Phase->TrySelectPattern(ERSBossPatternType::Basic, Selected);
	TestTrue(TEXT("Explicit request may repeat previous selection"), Selected == URSGameplayAbility_TargetedSlam::StaticClass());

	Phase->QueueDebugPattern(SlamName);
	Phase->RequestPersistentObjectCleanup();
	Phase->TrySelectPattern(ERSBossPatternType::Basic, Selected);
	TestTrue(TEXT("Cleanup cancels reservation"), Selected == URSGameplayAbility_ArmSwing::StaticClass());

	Phase->QueueDebugPattern(SlamName);
	Phase->AdvanceToNextPhase();
	Phase->TrySelectPattern(ERSBossPatternType::Basic, Selected);
	const auto FirstSelection = Selected;
	Phase->TrySelectPattern(ERSBossPatternType::Basic, Selected);
	TestTrue(TEXT("Next phase uses normal no-repeat policy"), Selected != FirstSelection);
	TestFalse(TEXT("Unknown candidate rejected"), Phase->QueueDebugPattern(TEXT("UnknownPattern")));
	TestTrue(TEXT("List does not queue a pattern"), Phase->QueueDebugPattern(FString()));
	return true;
}

#endif
