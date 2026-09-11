#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AbilitySystemComponent.h"
#include "RSBossCharacter.h"
#include "RSBossPhaseComponent.h"
#include "RSBossPhaseData.h"
#include "Tasks/RSAbilityTask_PendingFallingRock.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "RSFallingRocksTestTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSFallingRocksAbilityTest, "RS.Boss.FallingRocks.Ability", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSFallingRocksAbilityTest::RunTest(const FString& Parameters)
{
	const FName WorldName = MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("RSFallingRocksTestWorld"), EUniqueObjectNameOptions::GloballyUnique);
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false, WorldName, GetTransientPackage());
	TestWorld->AddToRoot();
	WorldContext.SetCurrentWorld(TestWorld);
	TestWorld->InitializeActorsForPlay(FURL());

	AddExpectedMessagePlain(TEXT("has no BehaviorTree"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	ARSBossCharacter* BossCharacter = TestWorld->SpawnActor<ARSBossCharacter>();
	TestNotNull(TEXT("Boss character"), BossCharacter);
	if (!BossCharacter)
	{
		TestWorld->RemoveFromRoot();
		GEngine->DestroyWorldContext(TestWorld);
		TestWorld->DestroyWorld(false);

		return false;
	}

	UAbilitySystemComponent* AbilitySystemComp = BossCharacter->GetAbilitySystemComponent();
	URSBossPhaseComponent* PhaseComponent = BossCharacter->GetBossPhaseComponent();
	TestNotNull(TEXT("Boss ability system"), AbilitySystemComp);
	TestNotNull(TEXT("Boss phase component"), PhaseComponent);

	FRSBossPhaseDefinition Phase;
	Phase.PersistentAbilityOnEnter = URSFallingRocksTestAbility::StaticClass();
	PhaseComponent->SetPhasesForTest({ Phase });

	AddExpectedMessagePlain(TEXT("Spec이 부여되지 않았습니다"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	TestFalse(TEXT("Phase entry fails when the configured ability was not granted"), PhaseComponent->EnterCurrentPhase());

	const FGameplayAbilitySpecHandle AbilityHandle = AbilitySystemComp->GiveAbility(FGameplayAbilitySpec(URSFallingRocksTestAbility::StaticClass(), 1));
	TestTrue(TEXT("Granted ability handle is valid"), AbilityHandle.IsValid());
	TestTrue(TEXT("Configured phase entry activates the granted ability"), PhaseComponent->EnterCurrentPhase());

	FGameplayAbilitySpec* AbilitySpec = AbilitySystemComp->FindAbilitySpecFromHandle(AbilityHandle);
	URSFallingRocksTestAbility* AbilityInstance = AbilitySpec ? Cast<URSFallingRocksTestAbility>(AbilitySpec->GetPrimaryInstance()) : nullptr;
	TestNotNull(TEXT("Instanced falling rocks ability"), AbilityInstance);
	if (AbilityInstance)
	{
		TestEqual(TEXT("Phase entry activates exactly once"), AbilityInstance->GetActivationCount(), 1);
		TestEqual(TEXT("Activation does not reserve the first rock immediately"), AbilityInstance->GetPendingRockCountForTest(), 0);

		AbilityInstance->AdvanceRockIntervalForTest();
		TestEqual(TEXT("First interval reserves the first rock"), AbilityInstance->GetPendingRockCountForTest(), 1);

		URSAbilityTask_PendingFallingRock* PendingRockTask = AbilityInstance->GetPendingRockForTest(0);
		TestNotNull(TEXT("Reserved pending rock"), PendingRockTask);
		if (PendingRockTask)
		{
			PendingRockTask->TriggerImpactTwiceForTest();
			TestEqual(TEXT("One rock commits impact exactly once"), PendingRockTask->GetCommittedImpactCountForTest(), 1);
			TestEqual(TEXT("Committed rock leaves the pending list"), AbilityInstance->GetPendingRockCountForTest(), 0);
		}

		AbilityInstance->AdvanceRockIntervalForTest();
		TestEqual(TEXT("A later interval can reserve another rock"), AbilityInstance->GetPendingRockCountForTest(), 1);

		TestTrue(TEXT("Repeated phase entry is an idempotent success"), PhaseComponent->EnterCurrentPhase());
		TestEqual(TEXT("Repeated phase entry does not activate a second instance"), AbilityInstance->GetActivationCount(), 1);

		AbilitySystemComp->CancelAbilityHandle(AbilityHandle);
		TestEqual(TEXT("Cancellation removes every pending rock"), AbilityInstance->GetPendingRockCountForTest(), 0);
	}

	TestWorld->RemoveFromRoot();
	GEngine->DestroyWorldContext(TestWorld);
	TestWorld->DestroyWorld(false);

	return true;
}

#endif
