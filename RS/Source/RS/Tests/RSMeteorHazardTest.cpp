#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Actors/RSBossMeteorHazard.h"
#include "Components/CapsuleComponent.h"
#include "Components/DecalComponent.h"
#include "Components/RSAttackTelegraphComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Materials/MaterialInterface.h"
#include "RSBossPhaseComponent.h"
#include "RSBossPhaseData.h"
#include "RSGameplayAbility_ConcentricRings.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSMeteorHazardDefinitionTest, "RS.Boss.MeteorHazard.Definition", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSMeteorHazardDefinitionTest::RunTest(const FString& Parameters)
{
	FRSBossMeteorHazardDefinition Definition;
	Definition.TelegraphDuration = 4.0f;
	Definition.FallEffectLeadTime = 3.0f;
	Definition.HazardRadius = 250.0f;
	Definition.DamageInterval = 1.5f;
	Definition.SpecialPatternCleanupOffset = 2;

	TestTrue(TEXT("Standard meteor hazard definition is valid"), Definition.IsDataValid());
	TestEqual(TEXT("Falling effect starts from impact minus lead time"), Definition.GetFallEffectStartTime(), 1.0f);
	TestFalse(TEXT("The first following special pattern keeps the hazard"), Definition.ShouldCleanupForSpecialPattern(7, 8));
	TestTrue(TEXT("The second following special pattern cleans the hazard"), Definition.ShouldCleanupForSpecialPattern(7, 9));
	TestFalse(TEXT("An older sequence cannot clean a newer hazard"), Definition.ShouldCleanupForSpecialPattern(9, 9));

	FRSBossMeteorHazardDefinition FullDurationLead = Definition;
	FullDurationLead.FallEffectLeadTime = FullDurationLead.TelegraphDuration;
	TestTrue(TEXT("Falling effect may start with the telegraph"), FullDurationLead.IsDataValid());
	TestEqual(TEXT("Full duration lead starts at zero"), FullDurationLead.GetFallEffectStartTime(), 0.0f);

	FRSBossMeteorHazardDefinition LateFallingEffect = Definition;
	LateFallingEffect.FallEffectLeadTime = 0.5f;
	TestTrue(TEXT("Falling effect may start after target lock"), LateFallingEffect.IsDataValid());
	TestEqual(TEXT("Short lead time remains measured from impact"), LateFallingEffect.GetFallEffectStartTime(), 3.5f);

	FRSBossMeteorHazardDefinition InvalidLead = Definition;
	InvalidLead.FallEffectLeadTime = InvalidLead.TelegraphDuration + 0.1f;
	TestFalse(TEXT("Lead time longer than telegraph duration is rejected"), InvalidLead.IsDataValid());

	FRSBossMeteorHazardDefinition InvalidDuration = Definition;
	InvalidDuration.TelegraphDuration = 0.0f;
	TestFalse(TEXT("Zero telegraph duration is rejected"), InvalidDuration.IsDataValid());

	FRSBossMeteorHazardDefinition InvalidDamageInterval = Definition;
	InvalidDamageInterval.DamageInterval = 0.0f;
	TestFalse(TEXT("Zero damage interval is rejected"), InvalidDamageInterval.IsDataValid());

	FRSBossMeteorHazardDefinition InvalidCleanupOffset = Definition;
	InvalidCleanupOffset.SpecialPatternCleanupOffset = 0;
	TestFalse(TEXT("Zero cleanup offset is rejected"), InvalidCleanupOffset.IsDataValid());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSMeteorHazardTimelineTest, "RS.Boss.MeteorHazard.Timeline", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSMeteorHazardTimelineTest::RunTest(const FString& Parameters)
{
	const FName WorldName = MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("RSMeteorHazardTestWorld"), EUniqueObjectNameOptions::GloballyUnique);
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false, WorldName, GetTransientPackage());
	TestWorld->AddToRoot();
	WorldContext.SetCurrentWorld(TestWorld);
	TestWorld->InitializeActorsForPlay(FURL());

	AActor* BossOwner = TestWorld->SpawnActor<AActor>();
	URSAttackTelegraphComponent* TelegraphComp = NewObject<URSAttackTelegraphComponent>(BossOwner);
	BossOwner->AddInstanceComponent(TelegraphComp);

	UMaterialInterface* TelegraphMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Resource/Telegraph/M_AttackTelegraph.M_AttackTelegraph"));
	FObjectProperty* DecalMaterialProperty = FindFProperty<FObjectProperty>(URSAttackTelegraphComponent::StaticClass(), TEXT("DecalMaterial"));
	TestNotNull(TEXT("Meteor test telegraph material"), TelegraphMaterial);
	TestNotNull(TEXT("Meteor test DecalMaterial property"), DecalMaterialProperty);
	if (TelegraphMaterial && DecalMaterialProperty)
	{
		DecalMaterialProperty->SetObjectPropertyValue_InContainer(TelegraphComp, TelegraphMaterial);
	}
	TelegraphComp->RegisterComponent();

	URSBossPhaseComponent* PhaseComponent = NewObject<URSBossPhaseComponent>(BossOwner);
	BossOwner->AddInstanceComponent(PhaseComponent);
	FRSBossPhaseDefinition Phase;
	Phase.SpecialPatterns = { URSGameplayAbility_ConcentricRings::StaticClass() };
	Phase.CycleSequence = { ERSBossPatternType::Special };
	TArray<FRSBossPhaseDefinition> Phases;
	Phases.Add(Phase);
	PhaseComponent->SetPhasesForTest(Phases);

	ACharacter* TargetCharacter = TestWorld->SpawnActor<ACharacter>(FVector(100.0f, 200.0f, 200.0f), FRotator::ZeroRotator);
	TestNotNull(TEXT("Meteor tracking target"), TargetCharacter);

	FRSBossMeteorHazardDefinition Definition;
	Definition.TelegraphDuration = 4.0f;
	Definition.FallEffectLeadTime = 3.0f;
	Definition.HazardRadius = 120.0f;
	Definition.DamageInterval = 1.5f;
	Definition.SpecialPatternCleanupOffset = 2;

	const FTransform SpawnTransform(TargetCharacter ? TargetCharacter->GetActorLocation() : FVector::ZeroVector);
	ARSBossMeteorHazard* MeteorHazard = TestWorld->SpawnActorDeferred<ARSBossMeteorHazard>(ARSBossMeteorHazard::StaticClass(), SpawnTransform, BossOwner, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	TestNotNull(TEXT("Meteor hazard actor"), MeteorHazard);
	if (!MeteorHazard || !TargetCharacter)
	{
		TestWorld->RemoveFromRoot();
		GEngine->DestroyWorldContext(TestWorld);
		TestWorld->DestroyWorld(false);

		return false;
	}

	MeteorHazard->SetDefinitionForTest(Definition);
	MeteorHazard->Initialize(TargetCharacter);
	MeteorHazard->FinishSpawning(SpawnTransform);
	if (!MeteorHazard->HasActorBegunPlay())
	{
		MeteorHazard->DispatchBeginPlay();
	}
	TestEqual(TEXT("Meteor starts by tracking"), MeteorHazard->GetMeteorHazardState(), ERSBossMeteorHazardState::Tracking);

	TargetCharacter->SetActorLocation(FVector(200.0f, 300.0f, 200.0f));
	MeteorHazard->Tick(1.0f);
	const FVector FirstTrackedBottom = TargetCharacter->GetCapsuleComponent()->GetComponentLocation() - FVector::UpVector * TargetCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	TestTrue(TEXT("Meteor tracks the target capsule bottom before 75 percent"), MeteorHazard->GetCurrentTargetLocation().Equals(FirstTrackedBottom));
	TestTrue(TEXT("Falling effect starts at impact minus lead time"), MeteorHazard->HasStartedFallingEffect());

	TargetCharacter->SetActorLocation(FVector(400.0f, -100.0f, 200.0f));
	MeteorHazard->Tick(2.0f);
	const FVector LockedBottom = TargetCharacter->GetCapsuleComponent()->GetComponentLocation() - FVector::UpVector * TargetCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	TestEqual(TEXT("Meteor locks exactly when progress reaches 75 percent"), MeteorHazard->GetMeteorHazardState(), ERSBossMeteorHazardState::Locked);
	TestTrue(TEXT("Locked impact location uses the last tracked capsule bottom"), MeteorHazard->GetLockedImpactLocation().Equals(LockedBottom));

	TargetCharacter->SetActorLocation(FVector(900.0f, 900.0f, 200.0f));
	MeteorHazard->Tick(0.5f);
	TestTrue(TEXT("Target movement after 75 percent does not move the impact"), MeteorHazard->GetLockedImpactLocation().Equals(LockedBottom));

	MeteorHazard->Tick(0.5f);
	TestEqual(TEXT("100 percent activates the persistent hazard"), MeteorHazard->GetMeteorHazardState(), ERSBossMeteorHazardState::Hazard);
	TestTrue(TEXT("Damage waits on an interval timer instead of applying at impact"), MeteorHazard->IsDamageTimerActiveForTest());

	MeteorHazard->Tick(100.0f);
	TestEqual(TEXT("Hazard has no individual natural lifetime"), MeteorHazard->GetMeteorHazardState(), ERSBossMeteorHazardState::Hazard);

	PhaseComponent->NotifyAbilityActivationRequested(URSGameplayAbility_ConcentricRings::StaticClass());
	TestEqual(TEXT("First following special pattern keeps the hazard"), MeteorHazard->GetMeteorHazardState(), ERSBossMeteorHazardState::Hazard);
	PhaseComponent->NotifyAbilityActivationRequested(URSGameplayAbility_ConcentricRings::StaticClass());
	TestEqual(TEXT("Second following special pattern cleans the hazard"), MeteorHazard->GetMeteorHazardState(), ERSBossMeteorHazardState::Cleanup);

	TestWorld->RemoveFromRoot();
	GEngine->DestroyWorldContext(TestWorld);
	TestWorld->DestroyWorld(false);

	return true;
}

#endif
