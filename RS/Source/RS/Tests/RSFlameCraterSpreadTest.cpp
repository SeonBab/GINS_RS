#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AbilitySystemComponent.h"
#include "Abilities/RSGameplayAbility_FlameCraterSpread.h"
#include "Actors/RSBossFlameCrater.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "RSGameplayTags.h"
#include "RSHealthSet.h"

namespace
{
	/** 화염 분화구 Actor 테스트용 임시 월드를 만들고 Play 상태로 초기화합니다 */
	UWorld* CreateFlameCraterTestWorld(FWorldContext*& OutWorldContext)
	{
		const FName WorldName = MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("RSFlameCraterTestWorld"), EUniqueObjectNameOptions::GloballyUnique);
		OutWorldContext = &GEngine->CreateNewWorldContext(EWorldType::Game);
		UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false, WorldName, GetTransientPackage());
		TestWorld->AddToRoot();
		OutWorldContext->SetCurrentWorld(TestWorld);
		TestWorld->InitializeActorsForPlay(FURL());
		TestWorld->BeginPlay();

		return TestWorld;
	}

	/** 테스트 월드가 남긴 WorldContext와 Root 참조를 함께 정리합니다 */
	void DestroyFlameCraterTestWorld(UWorld* TestWorld)
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
	void ApplyTestDamage(UAbilitySystemComponent* TargetAbilitySystemComponent, float DamageAmount)
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSFlameCraterSpreadPlacementTest, "RS.Boss.FlameCraterSpread.Placement", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSFlameCraterSpreadPlacementTest::RunTest(const FString& Parameters)
{
	FRSFlameCraterSpreadDefinition Definition;
	TestTrue(TEXT("Default spread definition is valid"), Definition.IsDataValid());

	const FVector Center(100.0f, 200.0f, 300.0f);
	FRandomStream FirstRandomStream(13579);
	TArray<FVector> FirstLocations;
	TestTrue(TEXT("Four landing locations are generated"), Definition.TryGenerateLandingLocations(Center, FVector::XAxisVector, FirstRandomStream, FirstLocations));
	TestEqual(TEXT("Exactly four FlameCraters are placed"), FirstLocations.Num(), FRSFlameCraterSpreadDefinition::FlameCraterCount);

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

	FRSFlameCraterSpreadDefinition InvalidDefinition = Definition;
	InvalidDefinition.MaxSpawnRadius = InvalidDefinition.MinSpawnRadius + 10.0f;
	TestFalse(TEXT("A range consumed by placement insets is invalid"), InvalidDefinition.IsDataValid());
	TestFalse(TEXT("A zero horizontal direction is rejected"), Definition.TryGenerateLandingLocations(Center, FVector::UpVector, SecondRandomStream, SecondLocations));

	FRSBossFlameCraterDefinition FlameCraterDefinition;
	TestFalse(TEXT("The next special pattern keeps the previous batch"), FlameCraterDefinition.ShouldCleanupForSpecialPattern(7, 8));
	TestTrue(TEXT("The second following special pattern cleans the previous batch"), FlameCraterDefinition.ShouldCleanupForSpecialPattern(7, 9));
	TestFalse(TEXT("A newer batch is not cleaned by an older sequence"), FlameCraterDefinition.ShouldCleanupForSpecialPattern(9, 9));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSBossFlameCraterHealthDamageTest, "RS.Boss.FlameCraterSpread.HealthDamage", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSBossFlameCraterHealthDamageTest::RunTest(const FString& Parameters)
{
	FWorldContext* WorldContext = nullptr;
	UWorld* TestWorld = CreateFlameCraterTestWorld(WorldContext);
	TestNotNull(TEXT("FlameCrater test world"), TestWorld);
	if (!TestWorld)
	{
		return false;
	}

	ARSBossFlameCrater* FlameCrater = TestWorld->SpawnActor<ARSBossFlameCrater>();
	TestNotNull(TEXT("FlameCrater actor"), FlameCrater);
	if (!FlameCrater)
	{
		DestroyFlameCraterTestWorld(TestWorld);

		return false;
	}
	FlameCrater->DispatchBeginPlay();

	FlameCrater->Tick(1.1f);
	TestEqual(TEXT("Finishing the fall starts charging"), FlameCrater->GetFlameCraterState(), ERSBossFlameCraterState::Charging);
	TestTrue(TEXT("Spawned FlameCrater has begun play"), FlameCrater->HasActorBegunPlay());
	UAbilitySystemComponent* FlameCraterAbilitySystem = FlameCrater->GetAbilitySystemComponent();
	TestNotNull(TEXT("FlameCrater owns an ability system"), FlameCraterAbilitySystem);
	if (!FlameCraterAbilitySystem)
	{
		DestroyFlameCraterTestWorld(TestWorld);

		return false;
	}
	TestTrue(TEXT("Charging applies the vulnerable state tag"), FlameCraterAbilitySystem->HasMatchingGameplayTag(RSGameplayTags::State_Hazard_FlameCrater_Vulnerable));
	TestEqual(TEXT("FlameCrater starts with three health"), FlameCraterAbilitySystem->GetNumericAttribute(URSHealthSet::GetHealthAttribute()), 3.0f);
	TestEqual(TEXT("FlameCrater starts with three max health"), FlameCraterAbilitySystem->GetNumericAttribute(URSHealthSet::GetMaxHealthAttribute()), 3.0f);
	FlameCraterAbilitySystem->AddLooseGameplayTag(RSGameplayTags::State_Immunity_Damage);
	ApplyTestDamage(FlameCraterAbilitySystem, 50.0f);
	TestEqual(TEXT("Damage immunity takes priority over unit damage"), FlameCraterAbilitySystem->GetNumericAttribute(URSHealthSet::GetHealthAttribute()), 3.0f);
	FlameCraterAbilitySystem->RemoveLooseGameplayTag(RSGameplayTags::State_Immunity_Damage);
	ApplyTestDamage(FlameCraterAbilitySystem, 0.0f);
	TestEqual(TEXT("Zero damage does not consume health"), FlameCraterAbilitySystem->GetNumericAttribute(URSHealthSet::GetHealthAttribute()), 3.0f);

	FlameCrater->Tick(3.1f);
	TestEqual(TEXT("Charge completion waits for a later frame before becoming a field"), FlameCrater->GetFlameCraterState(), ERSBossFlameCraterState::ChargeCompletedPending);

	ApplyTestDamage(FlameCraterAbilitySystem, 50.0f);
	ApplyTestDamage(FlameCraterAbilitySystem, 50.0f);
	TestEqual(TEXT("Each damage effect removes exactly one health"), FlameCraterAbilitySystem->GetNumericAttribute(URSHealthSet::GetHealthAttribute()), 1.0f);
	TestEqual(TEXT("Pending charge completion still accepts same-frame hits"), FlameCrater->GetFlameCraterState(), ERSBossFlameCraterState::ChargeCompletedPending);

	ApplyTestDamage(FlameCraterAbilitySystem, 50.0f);
	TestEqual(TEXT("The third same-frame damage reduces health to zero"), FlameCraterAbilitySystem->GetNumericAttribute(URSHealthSet::GetHealthAttribute()), 0.0f);
	TestEqual(TEXT("Zero health moves the actor to cleanup before field transition"), FlameCrater->GetFlameCraterState(), ERSBossFlameCraterState::Cleanup);

	DestroyFlameCraterTestWorld(TestWorld);

	return true;
}

#endif
