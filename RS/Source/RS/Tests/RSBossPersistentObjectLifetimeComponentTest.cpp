#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Components/RSBossPersistentObjectLifetimeComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "RSBossPhaseComponent.h"
#include "RSBossPhaseData.h"
#include "RSGameplayAbility_ConcentricRings.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRSBossPersistentObjectLifetimeComponentTest,
	"RS.Boss.PersistentObjectLifetime.Component",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSBossPersistentObjectLifetimeComponentTest::RunTest(const FString& Parameters)
{
	const FName WorldName = MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("RSPersistentObjectLifetimeTestWorld"), EUniqueObjectNameOptions::GloballyUnique);
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false, WorldName, GetTransientPackage());
	TestWorld->AddToRoot();
	WorldContext.SetCurrentWorld(TestWorld);
	TestWorld->InitializeActorsForPlay(FURL());
	TestWorld->BeginPlay();

	AActor* BossActor = TestWorld->SpawnActor<AActor>();
	URSBossPhaseComponent* PhaseComponent = NewObject<URSBossPhaseComponent>(BossActor);
	BossActor->AddInstanceComponent(PhaseComponent);
	PhaseComponent->RegisterComponent();

	FRSBossPhaseDefinition PhaseDefinition;
	PhaseDefinition.SpecialPatterns = { URSGameplayAbility_ConcentricRings::StaticClass() };
	PhaseDefinition.CycleSequence = { ERSBossPatternType::Special };
	PhaseComponent->SetPhasesForTest({ PhaseDefinition });

	AActor* PersistentActor = TestWorld->SpawnActor<AActor>();
	URSBossPersistentObjectLifetimeComponent* LifetimeComponent = NewObject<URSBossPersistentObjectLifetimeComponent>(PersistentActor);
	PersistentActor->AddInstanceComponent(LifetimeComponent);
	LifetimeComponent->RegisterComponent();

	int32 CleanupRequestCount = 0;
	LifetimeComponent->OnCleanupRequired().AddLambda([&CleanupRequestCount]()
	{
		++CleanupRequestCount;
	});

	TestFalse(TEXT("A non-positive cleanup offset cannot start observation"), LifetimeComponent->StartObserving(BossActor, 0));
	TestFalse(TEXT("Rejected observation leaves no phase subscription"), LifetimeComponent->IsObserving());
	TestTrue(TEXT("A valid boss and offset start observation"), LifetimeComponent->StartObserving(BossActor, 2));
	TestEqual(TEXT("Observation captures the current special-pattern sequence"), LifetimeComponent->GetSpawnSpecialPatternSequence(), 0);

	PhaseComponent->NotifyAbilityActivationRequested(URSGameplayAbility_ConcentricRings::StaticClass());
	TestEqual(TEXT("The first following special pattern keeps the object"), CleanupRequestCount, 0);
	TestTrue(TEXT("Lifetime observation remains active before its offset"), LifetimeComponent->IsObserving());

	PhaseComponent->NotifyAbilityActivationRequested(URSGameplayAbility_ConcentricRings::StaticClass());
	TestEqual(TEXT("The configured following special pattern requests cleanup once"), CleanupRequestCount, 1);
	TestFalse(TEXT("Cleanup request immediately releases phase subscriptions"), LifetimeComponent->IsObserving());

	PhaseComponent->NotifyAbilityActivationRequested(URSGameplayAbility_ConcentricRings::StaticClass());
	PhaseComponent->RequestPersistentObjectCleanup();
	TestEqual(TEXT("A completed lifetime never emits duplicate cleanup requests"), CleanupRequestCount, 1);

	TestTrue(TEXT("The same component can observe a later persistent object lifetime"), LifetimeComponent->StartObserving(BossActor, 2));
	TestEqual(TEXT("Restart captures the advanced special-pattern sequence"), LifetimeComponent->GetSpawnSpecialPatternSequence(), 3);
	PhaseComponent->RequestPersistentObjectCleanup();
	TestEqual(TEXT("The common cleanup signal requests immediate cleanup"), CleanupRequestCount, 2);
	TestFalse(TEXT("Common cleanup also releases phase subscriptions"), LifetimeComponent->IsObserving());

	TestTrue(TEXT("Observation can be started before an explicit stop"), LifetimeComponent->StartObserving(BossActor, 2));
	LifetimeComponent->StopObserving();
	PhaseComponent->RequestPersistentObjectCleanup();
	TestEqual(TEXT("Explicit stop prevents later phase cleanup callbacks"), CleanupRequestCount, 2);

	TestWorld->RemoveFromRoot();
	GEngine->DestroyWorldContext(TestWorld);
	TestWorld->DestroyWorld(false);

	return true;
}

#endif
