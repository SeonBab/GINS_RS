#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Actors/RSBossMeteorHazard.h"
#include "Components/CapsuleComponent.h"
#include "Components/DecalComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/RSAttackTelegraphComponent.h"
#include "Components/RSBossPersistentObjectLifetimeComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Materials/MaterialInterface.h"
#include "Combat/RSCombatFunctionLibrary.h"
#include "RSBossPhaseComponent.h"
#include "RSBossPhaseData.h"
#include "RSCombatShapeTestTypes.h"
#include "RSGameplayAbility_ConcentricRings.h"
#include "RSGameplayTags.h"
#include "RSPlayerCharacter.h"
#include "RSPlayerState.h"
#include "UObject/UnrealType.h"

namespace RSMeteorHazardTest
{
	/** 운석 Actor의 Timer와 Overlap을 함께 검증할 Play 상태 임시 월드를 만듭니다 */
	UWorld* CreateTestWorld()
	{
		const FName WorldName = MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("RSMeteorHazardIntegrationTestWorld"), EUniqueObjectNameOptions::GloballyUnique);
		FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
		UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false, WorldName, GetTransientPackage());
		TestWorld->AddToRoot();
		WorldContext.SetCurrentWorld(TestWorld);
		TestWorld->InitializeActorsForPlay(FURL());
		TestWorld->BeginPlay();

		return TestWorld;
	}

	/** 테스트 월드가 남긴 Root 참조와 WorldContext를 함께 정리합니다 */
	void DestroyTestWorld(UWorld* TestWorld)
	{
		if (!TestWorld)
		{
			return;
		}

		TestWorld->RemoveFromRoot();
		GEngine->DestroyWorldContext(TestWorld);
		TestWorld->DestroyWorld(false);
	}

	/** 운석 Actor가 실제 공용 Telegraph Handle을 사용하도록 테스트 소유자에 컴포넌트를 구성합니다 */
	URSAttackTelegraphComponent* AddTelegraphComponent(AActor* OwnerActor)
	{
		if (!OwnerActor)
		{
			return nullptr;
		}

		URSAttackTelegraphComponent* TelegraphComp = NewObject<URSAttackTelegraphComponent>(OwnerActor);
		OwnerActor->AddInstanceComponent(TelegraphComp);

		UMaterialInterface* TelegraphMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Resource/Telegraph/M_AttackTelegraph.M_AttackTelegraph"));
		FObjectProperty* DecalMaterialProperty = FindFProperty<FObjectProperty>(URSAttackTelegraphComponent::StaticClass(), TEXT("DecalMaterial"));
		if (!TelegraphMaterial || !DecalMaterialProperty)
		{
			return nullptr;
		}

		DecalMaterialProperty->SetObjectPropertyValue_InContainer(TelegraphComp, TelegraphMaterial);
		TelegraphComp->RegisterComponent();

		return TelegraphComp;
	}

	/** 수명 이벤트를 발행할 Phase Component를 테스트 소유자에 추가합니다 */
	URSBossPhaseComponent* AddPhaseComponent(AActor* OwnerActor)
	{
		if (!OwnerActor)
		{
			return nullptr;
		}

		URSBossPhaseComponent* PhaseComponent = NewObject<URSBossPhaseComponent>(OwnerActor);
		OwnerActor->AddInstanceComponent(PhaseComponent);

		FRSBossPhaseDefinition Phase;
		Phase.SpecialPatterns = { URSGameplayAbility_ConcentricRings::StaticClass() };
		Phase.CycleSequence = { ERSBossPatternType::Special };
		PhaseComponent->SetPhasesForTest({ Phase });

		return PhaseComponent;
	}

	/** 실제 Player Class와 PlayerState ASC를 함께 구성해 장판의 타입·사망 필터를 통과시킵니다 */
	ARSPlayerCharacter* SpawnPlayer(UWorld* TestWorld, const FVector& Location)
	{
		if (!TestWorld)
		{
			return nullptr;
		}

		ARSPlayerState* PlayerState = TestWorld->SpawnActor<ARSPlayerState>();
		ARSPlayerCharacter* PlayerCharacter = TestWorld->SpawnActor<ARSPlayerCharacter>(Location, FRotator::ZeroRotator);
		if (!PlayerState || !PlayerCharacter)
		{
			return nullptr;
		}

		PlayerCharacter->SetPlayerState(PlayerState);
		PlayerCharacter->InitializeAbilitySystem();

		return PlayerCharacter;
	}

	/** BeginPlay 전에 정의와 대상을 주입해 실제 운석 Actor 하나를 생성합니다 */
	ARSBossMeteorHazard* SpawnMeteorHazard(UWorld* TestWorld, AActor* OwnerActor, AActor* TargetActor, const FRSBossMeteorHazardDefinition& Definition)
	{
		if (!TestWorld || !OwnerActor || !TargetActor)
		{
			return nullptr;
		}

		const FTransform SpawnTransform(TargetActor->GetActorLocation());
		ARSBossMeteorHazard* MeteorHazard = TestWorld->SpawnActorDeferred<ARSBossMeteorHazard>(ARSBossMeteorHazard::StaticClass(), SpawnTransform, OwnerActor, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!MeteorHazard)
		{
			return nullptr;
		}

		MeteorHazard->SetDefinitionForTest(Definition);
		MeteorHazard->Initialize(TargetActor);
		MeteorHazard->FinishSpawning(SpawnTransform);
		if (!MeteorHazard->HasActorBegunPlay())
		{
			MeteorHazard->DispatchBeginPlay();
		}

		return MeteorHazard;
	}
}

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
	TestFalse(TEXT("The first following special pattern keeps the hazard"), URSBossPersistentObjectLifetimeComponent::ShouldCleanupForSpecialPattern(7, 8, Definition.SpecialPatternCleanupOffset));
	TestTrue(TEXT("The second following special pattern cleans the hazard"), URSBossPersistentObjectLifetimeComponent::ShouldCleanupForSpecialPattern(7, 9, Definition.SpecialPatternCleanupOffset));
	TestFalse(TEXT("An older sequence cannot clean a newer hazard"), URSBossPersistentObjectLifetimeComponent::ShouldCleanupForSpecialPattern(9, 9, Definition.SpecialPatternCleanupOffset));

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

	TargetCharacter->Destroy();
	MeteorHazard->Tick(0.5f);
	TestTrue(TEXT("Target loss after lock does not cancel the committed impact"), MeteorHazard->GetLockedImpactLocation().Equals(LockedBottom));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSMeteorHazardDamageAndCleanupTest, "RS.Boss.MeteorHazard.DamageAndCleanup", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSMeteorHazardDamageAndCleanupTest::RunTest(const FString& Parameters)
{
	UWorld* TestWorld = RSMeteorHazardTest::CreateTestWorld();
	TestNotNull(TEXT("Meteor integration test world"), TestWorld);
	if (!TestWorld)
	{
		return false;
	}

	ARSCombatShapeTestActor* BossOwner = TestWorld->SpawnActor<ARSCombatShapeTestActor>();
	URSAttackTelegraphComponent* TelegraphComp = RSMeteorHazardTest::AddTelegraphComponent(BossOwner);
	URSBossPhaseComponent* PhaseComponent = RSMeteorHazardTest::AddPhaseComponent(BossOwner);
	ARSPlayerCharacter* PlayerCharacter = RSMeteorHazardTest::SpawnPlayer(TestWorld, FVector(0.0f, 0.0f, 100.0f));
	UAbilitySystemComponent* BossAbilitySystemComp = BossOwner ? BossOwner->GetAbilitySystemComponent() : nullptr;
	TestNotNull(TEXT("Meteor owner"), BossOwner);
	TestNotNull(TEXT("Meteor owner telegraph"), TelegraphComp);
	TestNotNull(TEXT("Meteor owner phase"), PhaseComponent);
	TestNotNull(TEXT("Meteor player target"), PlayerCharacter);
	TestNotNull(TEXT("Meteor owner ability system"), BossAbilitySystemComp);
	TestNotNull(TEXT("Meteor player ability system"), PlayerCharacter ? PlayerCharacter->GetAbilitySystemComponent() : nullptr);
	TestEqual(
		TEXT("Meteor player capsule responds to PlayerHurtBox"),
		PlayerCharacter ? PlayerCharacter->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_GameTraceChannel1) : ECR_Ignore,
		ECR_Overlap);
	if (!BossOwner || !TelegraphComp || !PhaseComponent || !PlayerCharacter || !BossAbilitySystemComp)
	{
		RSMeteorHazardTest::DestroyTestWorld(TestWorld);

		return false;
	}

	int32 DamageEventCount = 0;
	TArray<const AActor*> DamageEventTargets;
	FGameplayEventMulticastDelegate& DamageEventDelegate = BossAbilitySystemComp->GenericGameplayEventCallbacks.FindOrAdd(RSGameplayTags::GameplayEvent_Combat_MeteorHazardDamage);
	const FDelegateHandle DamageEventDelegateHandle = DamageEventDelegate.AddLambda([&DamageEventCount, &DamageEventTargets](const FGameplayEventData* EventData)
	{
		++DamageEventCount;
		DamageEventTargets.Add(EventData ? EventData->Target.Get() : nullptr);
	});
	FGameplayEventData ProbePayload;
	ProbePayload.EventTag = RSGameplayTags::GameplayEvent_Combat_MeteorHazardDamage;
	ProbePayload.Target = PlayerCharacter;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(BossOwner, ProbePayload.EventTag, ProbePayload);
	TestEqual(TEXT("Meteor owner ASC forwards the damage gameplay event"), DamageEventCount, 1);
	DamageEventCount = 0;
	DamageEventTargets.Reset();

	FRSBossMeteorHazardDefinition Definition;
	Definition.TelegraphDuration = 0.1f;
	Definition.FallEffectLeadTime = 0.1f;
	Definition.HazardRadius = 100.0f;
	Definition.DamageInterval = 0.25f;
	Definition.SpecialPatternCleanupOffset = 2;

	ARSBossMeteorHazard* FirstMeteorHazard = RSMeteorHazardTest::SpawnMeteorHazard(TestWorld, BossOwner, PlayerCharacter, Definition);
	ARSBossMeteorHazard* SecondMeteorHazard = RSMeteorHazardTest::SpawnMeteorHazard(TestWorld, BossOwner, PlayerCharacter, Definition);
	TestNotNull(TEXT("First overlapping meteor hazard"), FirstMeteorHazard);
	TestNotNull(TEXT("Second overlapping meteor hazard"), SecondMeteorHazard);
	if (!FirstMeteorHazard || !SecondMeteorHazard)
	{
		DamageEventDelegate.Remove(DamageEventDelegateHandle);
		RSMeteorHazardTest::DestroyTestWorld(TestWorld);

		return false;
	}

	FirstMeteorHazard->Tick(Definition.TelegraphDuration);
	SecondMeteorHazard->Tick(Definition.TelegraphDuration);
	TestEqual(TEXT("First meteor becomes a hazard"), FirstMeteorHazard->GetMeteorHazardState(), ERSBossMeteorHazardState::Hazard);
	TestEqual(TEXT("Second meteor becomes a hazard"), SecondMeteorHazard->GetMeteorHazardState(), ERSBossMeteorHazardState::Hazard);
	TestTrue(TEXT("First meteor schedules its periodic damage timer"), FirstMeteorHazard->IsDamageTimerActiveForTest());
	TestTrue(TEXT("Second meteor schedules its periodic damage timer"), SecondMeteorHazard->IsDamageTimerActiveForTest());
	TestTrue(TEXT("First damage waits for exactly one configured interval"), FMath::IsNearlyEqual(FirstMeteorHazard->GetDamageTimerRemainingForTest(), Definition.DamageInterval));
	TestEqual(TEXT("Hazard transition applies no immediate damage"), DamageEventCount, 0);
	FRSCombatShape ProbeShape;
	ProbeShape.Type = ERSCombatShapeType::Sphere;
	ProbeShape.Radius = Definition.HazardRadius;
	ProbeShape.InnerRadius = 0.0f;
	TArray<AActor*> ProbeTargets;
	URSCombatFunctionLibrary::FindTargetsInShape(FirstMeteorHazard, ECC_GameTraceChannel1, ProbeShape, FTransform(FirstMeteorHazard->GetLockedImpactLocation()), ProbeTargets);
	TestTrue(TEXT("Shared combat shape finds the player inside the meteor radius"), ProbeTargets.Contains(PlayerCharacter));

	FirstMeteorHazard->ApplyHazardDamageForTest();
	SecondMeteorHazard->ApplyHazardDamageForTest();
	TestEqual(TEXT("Two overlapping hazards send two independent damage events"), DamageEventCount, 2);
	TestTrue(TEXT("Both overlapping events target the player"), DamageEventTargets.Num() == 2 && DamageEventTargets[0] == PlayerCharacter && DamageEventTargets[1] == PlayerCharacter);

	const FVector LockedImpactLocation = FirstMeteorHazard->GetLockedImpactLocation();
	PlayerCharacter->SetActorLocation(LockedImpactLocation + FVector(Definition.HazardRadius, 0.0f, 100.0f));
	FirstMeteorHazard->ApplyHazardDamageForTest();
	SecondMeteorHazard->ApplyHazardDamageForTest();
	TestEqual(TEXT("The exact outer radius remains excluded"), DamageEventCount, 2);

	PlayerCharacter->SetActorLocation(LockedImpactLocation + FVector(Definition.HazardRadius - 1.0f, 0.0f, 100.0f));
	FirstMeteorHazard->ApplyHazardDamageForTest();
	SecondMeteorHazard->ApplyHazardDamageForTest();
	TestEqual(TEXT("Each overlapping hazard damages once per interval inside the radius"), DamageEventCount, 4);

	PlayerCharacter->SetActorLocation(LockedImpactLocation + FVector(Definition.HazardRadius + 100.0f, 0.0f, 100.0f));
	ARSCombatShapeTestActor* NonPlayerTarget = TestWorld->SpawnActor<ARSCombatShapeTestActor>(LockedImpactLocation, FRotator::ZeroRotator);
	UPrimitiveComponent* NonPlayerCollisionComp = NonPlayerTarget ? Cast<UPrimitiveComponent>(NonPlayerTarget->GetRootComponent()) : nullptr;
	if (NonPlayerCollisionComp)
	{
		NonPlayerCollisionComp->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Overlap);
	}
	TestNotNull(TEXT("Non-player overlap target"), NonPlayerCollisionComp);
	FirstMeteorHazard->ApplyHazardDamageForTest();
	SecondMeteorHazard->ApplyHazardDamageForTest();
	TestEqual(TEXT("A non-player inside the hazard receives no damage event"), DamageEventCount, 4);

	PhaseComponent->NotifyAbilityActivationRequested(URSGameplayAbility_ConcentricRings::StaticClass());
	TestEqual(TEXT("First following special request keeps the first hazard"), FirstMeteorHazard->GetMeteorHazardState(), ERSBossMeteorHazardState::Hazard);
	TestEqual(TEXT("First following special request keeps the second hazard"), SecondMeteorHazard->GetMeteorHazardState(), ERSBossMeteorHazardState::Hazard);
	PhaseComponent->NotifyAbilityActivationRequested(URSGameplayAbility_ConcentricRings::StaticClass());
	TestEqual(TEXT("Second following special request cleans the first hazard"), FirstMeteorHazard->GetMeteorHazardState(), ERSBossMeteorHazardState::Cleanup);
	TestEqual(TEXT("Second following special request cleans the second hazard"), SecondMeteorHazard->GetMeteorHazardState(), ERSBossMeteorHazardState::Cleanup);
	TestFalse(TEXT("First cleaned hazard stops its damage timer"), FirstMeteorHazard->IsDamageTimerActiveForTest());
	TestFalse(TEXT("Second cleaned hazard stops its damage timer"), SecondMeteorHazard->IsDamageTimerActiveForTest());

	PlayerCharacter->SetActorLocation(FVector(0.0f, 0.0f, 100.0f));
	ARSBossMeteorHazard* PersistentCleanupHazard = RSMeteorHazardTest::SpawnMeteorHazard(TestWorld, BossOwner, PlayerCharacter, Definition);
	TestNotNull(TEXT("Common cleanup meteor hazard"), PersistentCleanupHazard);
	if (PersistentCleanupHazard)
	{
		PhaseComponent->RequestPersistentObjectCleanup();
		TestEqual(TEXT("Main gimmick, boss death, and encounter cleanup event removes the hazard"), PersistentCleanupHazard->GetMeteorHazardState(), ERSBossMeteorHazardState::Cleanup);
	}

	ARSPlayerCharacter* LostPlayerCharacter = RSMeteorHazardTest::SpawnPlayer(TestWorld, FVector(300.0f, 0.0f, 100.0f));
	ARSBossMeteorHazard* TrackingHazard = RSMeteorHazardTest::SpawnMeteorHazard(TestWorld, BossOwner, LostPlayerCharacter, Definition);
	TestNotNull(TEXT("Tracking cleanup meteor hazard"), TrackingHazard);
	if (LostPlayerCharacter && TrackingHazard)
	{
		LostPlayerCharacter->Destroy();
		TrackingHazard->Tick(Definition.TelegraphDuration * 0.5f);
		TestEqual(TEXT("Target loss before 75 percent cleans the tracking hazard"), TrackingHazard->GetMeteorHazardState(), ERSBossMeteorHazardState::Cleanup);
	}

	DamageEventDelegate.Remove(DamageEventDelegateHandle);
	RSMeteorHazardTest::DestroyTestWorld(TestWorld);

	return true;
}

#endif
