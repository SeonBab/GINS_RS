#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Boss/RSGameplayAbility_FireballSpread.h"
#include "Actors/RSBossFireball.h"
#include "Components/RSBossPersistentObjectLifetimeComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "RSGameplayTags.h"
#include "RSHealthSet.h"

namespace
{
	/** 화염구 Actor 테스트용 임시 월드를 만들고 Play 상태로 초기화합니다 */
	UWorld* CreateFireballTestWorld(FWorldContext*& OutWorldContext)
	{
		const FName WorldName = MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("RSFireballTestWorld"), EUniqueObjectNameOptions::GloballyUnique);
		OutWorldContext = &GEngine->CreateNewWorldContext(EWorldType::Game);
		UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false, WorldName, GetTransientPackage());
		TestWorld->AddToRoot();
		OutWorldContext->SetCurrentWorld(TestWorld);
		TestWorld->InitializeActorsForPlay(FURL());
		TestWorld->BeginPlay();

		return TestWorld;
	}

	/** 테스트 월드가 남긴 WorldContext와 Root 참조를 함께 정리합니다 */
	void DestroyFireballTestWorld(UWorld* TestWorld)
	{
		if (!TestWorld)
		{
			return;
		}

		TestWorld->RemoveFromRoot();
		GEngine->DestroyWorldContext(TestWorld);
		TestWorld->DestroyWorld(false);
	}

	/** 실제 공격과 같은 Instant Damage GameplayEffect를 대상 ASC에 적용합니다 */
	void ApplyFireballTestDamage(UAbilitySystemComponent* TargetAbilitySystemComponent, float DamageAmount)
	{
		UGameplayEffect* DamageEffect = NewObject<UGameplayEffect>(GetTransientPackage());
		DamageEffect->DurationPolicy = EGameplayEffectDurationType::Instant;

		FGameplayModifierInfo DamageModifier;
		DamageModifier.Attribute = URSHealthSet::GetDamageAttribute();
		DamageModifier.ModifierOp = EGameplayModOp::Additive;
		DamageModifier.ModifierMagnitude = FScalableFloat(DamageAmount);
		DamageEffect->Modifiers.Add(DamageModifier);

		FGameplayEffectContextHandle EffectContext = TargetAbilitySystemComponent->MakeEffectContext();
		FGameplayEffectSpec DamageSpec(DamageEffect, EffectContext, 1.0f);
		TargetAbilitySystemComponent->ApplyGameplayEffectSpecToSelf(DamageSpec);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSFireballSpreadPlacementTest, "RS.Boss.FireballSpread.Placement", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSFireballSpreadPlacementTest::RunTest(const FString& Parameters)
{
	FRSFireballSpreadDefinition Definition;
	TestTrue(TEXT("Default spread definition is valid"), Definition.IsDataValid());

	const FVector Center(100.0f, 200.0f, 300.0f);
	FRandomStream FirstRandomStream(13579);
	TArray<FVector> FirstLocations;
	TestTrue(TEXT("Four landing locations are generated"), Definition.TryGenerateLandingLocations(Center, FVector::XAxisVector, FirstRandomStream, FirstLocations));
	TestEqual(TEXT("Exactly four Fireballs are placed"), FirstLocations.Num(), FRSFireballSpreadDefinition::FireballCount);

	const float EffectiveMinimumRadius = Definition.MinSpawnRadius + Definition.PlacementRadius + Definition.BoundaryMargin;
	const float EffectiveMaximumRadius = Definition.MaxSpawnRadius - Definition.PlacementRadius - Definition.BoundaryMargin;
	for (int32 LocationIndex = 0; LocationIndex < FirstLocations.Num(); ++LocationIndex)
	{
		const FVector Offset = FirstLocations[LocationIndex] - Center;
		const float Radius = FVector2D(Offset.X, Offset.Y).Size();
		const float Angle = FMath::RadiansToDegrees(FMath::Atan2(Offset.Y, Offset.X));
		const float SectorMinimumAngle = -90.0f + LocationIndex * 45.0f;
		const float SectorMaximumAngle = SectorMinimumAngle + 45.0f;

		TestTrue(FString::Printf(TEXT("Location %d stays inside its radial range"), LocationIndex), Radius >= EffectiveMinimumRadius && Radius <= EffectiveMaximumRadius);
		TestTrue(FString::Printf(TEXT("Location %d stays inside its 45 degree sector"), LocationIndex), Angle > SectorMinimumAngle && Angle < SectorMaximumAngle);
		TestTrue(FString::Printf(TEXT("Location %d stays on the captured floor plane"), LocationIndex), FMath::IsNearlyEqual(FirstLocations[LocationIndex].Z, Center.Z));
	}

	const float RequiredCenterDistance = Definition.PlacementRadius * 2.0f + Definition.MinimumGap;
	for (int32 FirstIndex = 0; FirstIndex < FirstLocations.Num(); ++FirstIndex)
	{
		for (int32 SecondIndex = FirstIndex + 1; SecondIndex < FirstLocations.Num(); ++SecondIndex)
		{
			const float Distance = FVector::Dist2D(FirstLocations[FirstIndex], FirstLocations[SecondIndex]);
			TestTrue(FString::Printf(TEXT("Locations %d and %d preserve the configured gap"), FirstIndex, SecondIndex), Distance >= RequiredCenterDistance);
		}
	}

	FRandomStream SecondRandomStream(13579);
	TArray<FVector> SecondLocations;
	TestTrue(TEXT("The same seed generates another valid set"), Definition.TryGenerateLandingLocations(Center, FVector::XAxisVector, SecondRandomStream, SecondLocations));
	TestTrue(TEXT("The same seed is deterministic"), FirstLocations == SecondLocations);

	FRSFireballSpreadDefinition InvalidDefinition = Definition;
	InvalidDefinition.MaxSpawnRadius = InvalidDefinition.MinSpawnRadius + 10.0f;
	TestFalse(TEXT("A range consumed by placement insets is invalid"), InvalidDefinition.IsDataValid());
	TestFalse(TEXT("A zero horizontal direction is rejected"), Definition.TryGenerateLandingLocations(Center, FVector::UpVector, SecondRandomStream, SecondLocations));

	FRSBossFireballDefinition FireballDefinition;
	TestFalse(TEXT("The next special pattern keeps the previous batch"), URSBossPersistentObjectLifetimeComponent::ShouldCleanupForSpecialPattern(7, 8, FireballDefinition.SpecialPatternCleanupOffset));
	TestTrue(TEXT("The second following special pattern cleans the previous batch"), URSBossPersistentObjectLifetimeComponent::ShouldCleanupForSpecialPattern(7, 9, FireballDefinition.SpecialPatternCleanupOffset));
	TestFalse(TEXT("A newer batch is not cleaned by an older sequence"), URSBossPersistentObjectLifetimeComponent::ShouldCleanupForSpecialPattern(9, 9, FireballDefinition.SpecialPatternCleanupOffset));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSBossFireballHealthDamageTest, "RS.Boss.FireballSpread.HealthDamage", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSBossFireballHealthDamageTest::RunTest(const FString& Parameters)
{
	FWorldContext* WorldContext = nullptr;
	UWorld* TestWorld = CreateFireballTestWorld(WorldContext);
	TestNotNull(TEXT("Fireball test world"), TestWorld);
	if (!TestWorld)
	{
		return false;
	}

	ARSBossFireball* Fireball = TestWorld->SpawnActor<ARSBossFireball>();
	TestNotNull(TEXT("Fireball actor"), Fireball);
	if (!Fireball)
	{
		DestroyFireballTestWorld(TestWorld);

		return false;
	}
	Fireball->DispatchBeginPlay();

	Fireball->Tick(1.1f);
	TestEqual(TEXT("Finishing the fall starts charging"), Fireball->GetFireballState(), ERSBossFireballState::Charging);
	TestTrue(TEXT("Spawned Fireball has begun play"), Fireball->HasActorBegunPlay());
	UAbilitySystemComponent* FireballAbilitySystem = Fireball->GetAbilitySystemComponent();
	TestNotNull(TEXT("Fireball owns an ability system"), FireballAbilitySystem);
	if (!FireballAbilitySystem)
	{
		DestroyFireballTestWorld(TestWorld);

		return false;
	}
	TestTrue(TEXT("Charging applies the vulnerable state tag"), FireballAbilitySystem->HasMatchingGameplayTag(RSGameplayTags::State_Hazard_Fireball_Vulnerable));
	TestEqual(TEXT("Fireball starts with three health"), FireballAbilitySystem->GetNumericAttribute(URSHealthSet::GetHealthAttribute()), 3.0f);
	TestEqual(TEXT("Fireball starts with three max health"), FireballAbilitySystem->GetNumericAttribute(URSHealthSet::GetMaxHealthAttribute()), 3.0f);
	FireballAbilitySystem->AddLooseGameplayTag(RSGameplayTags::State_Immunity_Damage);
	ApplyFireballTestDamage(FireballAbilitySystem, 50.0f);
	TestEqual(TEXT("Damage immunity takes priority over unit damage"), FireballAbilitySystem->GetNumericAttribute(URSHealthSet::GetHealthAttribute()), 3.0f);
	FireballAbilitySystem->RemoveLooseGameplayTag(RSGameplayTags::State_Immunity_Damage);
	ApplyFireballTestDamage(FireballAbilitySystem, 0.0f);
	TestEqual(TEXT("Zero damage does not consume health"), FireballAbilitySystem->GetNumericAttribute(URSHealthSet::GetHealthAttribute()), 3.0f);

	Fireball->Tick(3.1f);
	TestEqual(TEXT("Charge completion waits for a later frame before becoming a field"), Fireball->GetFireballState(), ERSBossFireballState::ChargeCompletedPending);

	ApplyFireballTestDamage(FireballAbilitySystem, 50.0f);
	ApplyFireballTestDamage(FireballAbilitySystem, 50.0f);
	TestEqual(TEXT("Each damage effect removes exactly one health"), FireballAbilitySystem->GetNumericAttribute(URSHealthSet::GetHealthAttribute()), 1.0f);
	TestEqual(TEXT("Pending charge completion still accepts same-frame hits"), Fireball->GetFireballState(), ERSBossFireballState::ChargeCompletedPending);

	ApplyFireballTestDamage(FireballAbilitySystem, 50.0f);
	TestEqual(TEXT("The third same-frame damage reduces health to zero"), FireballAbilitySystem->GetNumericAttribute(URSHealthSet::GetHealthAttribute()), 0.0f);
	TestEqual(TEXT("Zero health moves the actor to cleanup before field transition"), Fireball->GetFireballState(), ERSBossFireballState::Cleanup);

	DestroyFireballTestWorld(TestWorld);

	return true;
}

#endif
