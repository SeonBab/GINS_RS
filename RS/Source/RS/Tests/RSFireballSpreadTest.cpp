#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Boss/RSGameplayAbility_FireballSpread.h"
#include "Actors/RSBossFireball.h"
#include "Components/DecalComponent.h"
#include "Components/RSAttackTelegraphComponent.h"
#include "Components/RSBossPersistentObjectLifetimeComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "RSGameplayTags.h"
#include "RSHealthSet.h"
#include "UObject/UnrealType.h"

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

	/** Blueprint에서 지정할 공용 Telegraph Material을 테스트에서 대신 주입합니다 */
	bool ApplyFireballTelegraphMaterial(AActor* FireballActor)
	{
		URSAttackTelegraphComponent* TelegraphComp = FireballActor ? FireballActor->FindComponentByClass<URSAttackTelegraphComponent>() : nullptr;
		UMaterialInterface* TelegraphMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Resource/Telegraph/M_AttackTelegraph.M_AttackTelegraph"));
		FObjectProperty* DecalMaterialProperty = FindFProperty<FObjectProperty>(URSAttackTelegraphComponent::StaticClass(), TEXT("DecalMaterial"));
		if (!TelegraphComp || !TelegraphMaterial || !DecalMaterialProperty)
		{
			return false;
		}

		DecalMaterialProperty->SetObjectPropertyValue_InContainer(TelegraphComp, TelegraphMaterial);

		return true;
	}

	/** 화염구 Actor가 소유한 표시 슬롯의 Dynamic Material Instance를 찾습니다 */
	UMaterialInstanceDynamic* FindVisibleTelegraphMaterialInstance(const AActor* FireballActor)
	{
		if (!FireballActor)
		{
			return nullptr;
		}

		TArray<UDecalComponent*> Decals;
		FireballActor->GetComponents<UDecalComponent>(Decals);
		for (UDecalComponent* Decal : Decals)
		{
			if (Decal && Decal->IsVisible())
			{
				return Cast<UMaterialInstanceDynamic>(Decal->GetDecalMaterial());
			}
		}

		return nullptr;
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

	/** 생성 Ability가 전달하는 것과 같은 유효한 장판 적중 스냅샷을 만듭니다 */
	FRSBossPatternHitSpec MakeFireballHitSpec()
	{
		FRSBossPatternHitSpec HitSpec;
		HitSpec.DamageAmount = 50.0f;
		HitSpec.DamageEffectClass = LoadClass<UGameplayEffect>(nullptr, TEXT("/Game/AbilitySystem/GameplayEffects/GE_Damage_Basic.GE_Damage_Basic_C"));

		return HitSpec;
	}

	/** 설정한 개수만큼 180도를 균등 분할하고 각 구역에 화염구 하나씩 배치했는지 검사합니다 */
	void VerifyLandingLocations(FAutomationTestBase& Test, const FRSFireballSpreadDefinition& Definition)
	{
		const FVector Center(100.0f, 200.0f, 300.0f);

		// 배치 코드와 같은 식을 재사용하지 않도록 기대하는 구역 각도를 테스트에서 직접 계산합니다
		const float SectorAngle = 180.0f / Definition.FireballCount;
		const float EffectiveMinimumRadius = Definition.MinSpawnRadius + Definition.PlacementRadius + Definition.BoundaryMargin;
		const float EffectiveMaximumRadius = Definition.MaxSpawnRadius - Definition.PlacementRadius - Definition.BoundaryMargin;
		const float RequiredCenterDistance = Definition.PlacementRadius * 2.0f + Definition.MinimumGap;

		FRandomStream FirstRandomStream(13579);
		TArray<FVector> FirstLocations;
		Test.TestTrue(FString::Printf(TEXT("Count %d generates landing locations"), Definition.FireballCount), Definition.TryGenerateLandingLocations(Center, FVector::XAxisVector, FirstRandomStream, FirstLocations));
		Test.TestEqual(FString::Printf(TEXT("Count %d places exactly that many Fireballs"), Definition.FireballCount), FirstLocations.Num(), Definition.FireballCount);
		if (FirstLocations.Num() != Definition.FireballCount)
		{
			return;
		}

		for (int32 LocationIndex = 0; LocationIndex < FirstLocations.Num(); ++LocationIndex)
		{
			const FVector Offset = FirstLocations[LocationIndex] - Center;
			const float Radius = FVector2D(Offset.X, Offset.Y).Size();
			const float Angle = FMath::RadiansToDegrees(FMath::Atan2(Offset.Y, Offset.X));
			const float SectorMinimumAngle = -90.0f + LocationIndex * SectorAngle;
			const float SectorMaximumAngle = SectorMinimumAngle + SectorAngle;

			Test.TestTrue(FString::Printf(TEXT("Count %d location %d stays inside its radial range"), Definition.FireballCount, LocationIndex), Radius >= EffectiveMinimumRadius && Radius <= EffectiveMaximumRadius);
			Test.TestTrue(FString::Printf(TEXT("Count %d location %d stays inside its sector"), Definition.FireballCount, LocationIndex), Angle > SectorMinimumAngle && Angle < SectorMaximumAngle);
			Test.TestTrue(FString::Printf(TEXT("Count %d location %d stays on the captured floor plane"), Definition.FireballCount, LocationIndex), FMath::IsNearlyEqual(FirstLocations[LocationIndex].Z, Center.Z));
		}

		for (int32 FirstIndex = 0; FirstIndex < FirstLocations.Num(); ++FirstIndex)
		{
			for (int32 SecondIndex = FirstIndex + 1; SecondIndex < FirstLocations.Num(); ++SecondIndex)
			{
				const float Distance = FVector::Dist2D(FirstLocations[FirstIndex], FirstLocations[SecondIndex]);
				Test.TestTrue(FString::Printf(TEXT("Count %d locations %d and %d preserve the configured gap"), Definition.FireballCount, FirstIndex, SecondIndex), Distance >= RequiredCenterDistance);
			}
		}

		FRandomStream SecondRandomStream(13579);
		TArray<FVector> SecondLocations;
		Test.TestTrue(FString::Printf(TEXT("Count %d regenerates from the same seed"), Definition.FireballCount), Definition.TryGenerateLandingLocations(Center, FVector::XAxisVector, SecondRandomStream, SecondLocations));
		Test.TestTrue(FString::Printf(TEXT("Count %d is deterministic for the same seed"), Definition.FireballCount), FirstLocations == SecondLocations);
	}

	/** BeginPlay 전에 Ability 소유 설정을 주입해 실제 화염구 생성 경로를 재현합니다 */
	ARSBossFireball* SpawnFireball(UWorld* TestWorld, const FRSBossFireballDefinition& Definition, bool bApplyTelegraphMaterial)
	{
		if (!TestWorld)
		{
			return nullptr;
		}

		const FTransform SpawnTransform = FTransform::Identity;
		ARSBossFireball* Fireball = TestWorld->SpawnActorDeferred<ARSBossFireball>(ARSBossFireball::StaticClass(), SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Fireball)
		{
			return nullptr;
		}

		Fireball->Initialize(Definition, MakeFireballHitSpec());
		if (bApplyTelegraphMaterial && !ApplyFireballTelegraphMaterial(Fireball))
		{
			return nullptr;
		}

		Fireball->FinishSpawning(SpawnTransform);

		return Fireball;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSFireballSpreadPlacementTest, "RS.Boss.FireballSpread.Placement", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSFireballSpreadPlacementTest::RunTest(const FString& Parameters)
{
	FRSFireballSpreadDefinition Definition;
	TestTrue(TEXT("Default spread definition is valid"), Definition.IsDataValid());

	VerifyLandingLocations(*this, Definition);

	// 기획자가 개수를 바꾸면 구역 수와 각 구역의 폭이 함께 따라갑니다
	FRSFireballSpreadDefinition ThreeFireballDefinition = Definition;
	ThreeFireballDefinition.FireballCount = 3;
	TestTrue(TEXT("Three Fireballs fit the default radii"), ThreeFireballDefinition.IsDataValid());
	VerifyLandingLocations(*this, ThreeFireballDefinition);

	// 구역이 좁아지는 만큼 안쪽 반경이 넓어야 화염구 하나가 들어갈 각도가 남습니다
	FRSFireballSpreadDefinition SixFireballDefinition = Definition;
	SixFireballDefinition.FireballCount = 6;
	TestFalse(TEXT("Six Fireballs do not fit the default inner radius"), SixFireballDefinition.IsDataValid());
	SixFireballDefinition.MinSpawnRadius = 400.0f;
	TestTrue(TEXT("Six Fireballs fit a wider inner radius"), SixFireballDefinition.IsDataValid());
	VerifyLandingLocations(*this, SixFireballDefinition);

	FRSFireballSpreadDefinition EmptyDefinition = Definition;
	EmptyDefinition.FireballCount = 0;
	TestFalse(TEXT("A spread without Fireballs is invalid"), EmptyDefinition.IsDataValid());

	FRSFireballSpreadDefinition InvalidDefinition = Definition;
	InvalidDefinition.MaxSpawnRadius = InvalidDefinition.MinSpawnRadius + 10.0f;
	TestFalse(TEXT("A range consumed by placement insets is invalid"), InvalidDefinition.IsDataValid());

	FRandomStream RejectedRandomStream(13579);
	TArray<FVector> RejectedLocations;
	TestFalse(TEXT("A zero horizontal direction is rejected"), Definition.TryGenerateLandingLocations(FVector(100.0f, 200.0f, 300.0f), FVector::UpVector, RejectedRandomStream, RejectedLocations));

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

	FRSBossFireballDefinition FireballDefinition;
	FireballDefinition.RequiredHitCount = 4;
	ARSBossFireball* Fireball = SpawnFireball(TestWorld, FireballDefinition, false);
	TestNotNull(TEXT("Fireball actor"), Fireball);
	if (!Fireball)
	{
		DestroyFireballTestWorld(TestWorld);

		return false;
	}

	const float ConfiguredHitPoints = static_cast<float>(FireballDefinition.RequiredHitCount);

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
	TestEqual(TEXT("Fireball starts with the configured health"), FireballAbilitySystem->GetNumericAttribute(URSHealthSet::GetHealthAttribute()), ConfiguredHitPoints);
	TestEqual(TEXT("Fireball starts with the configured max health"), FireballAbilitySystem->GetNumericAttribute(URSHealthSet::GetMaxHealthAttribute()), ConfiguredHitPoints);
	FireballAbilitySystem->AddLooseGameplayTag(RSGameplayTags::State_Immunity_Damage);
	ApplyFireballTestDamage(FireballAbilitySystem, 50.0f);
	TestEqual(TEXT("Damage immunity takes priority over unit damage"), FireballAbilitySystem->GetNumericAttribute(URSHealthSet::GetHealthAttribute()), ConfiguredHitPoints);
	FireballAbilitySystem->RemoveLooseGameplayTag(RSGameplayTags::State_Immunity_Damage);
	ApplyFireballTestDamage(FireballAbilitySystem, 0.0f);
	TestEqual(TEXT("Zero damage does not consume health"), FireballAbilitySystem->GetNumericAttribute(URSHealthSet::GetHealthAttribute()), ConfiguredHitPoints);

	Fireball->Tick(3.1f);
	TestEqual(TEXT("Charge completion waits for a later frame before becoming a field"), Fireball->GetFireballState(), ERSBossFireballState::ChargeCompletedPending);

	ApplyFireballTestDamage(FireballAbilitySystem, 50.0f);
	ApplyFireballTestDamage(FireballAbilitySystem, 50.0f);
	ApplyFireballTestDamage(FireballAbilitySystem, 50.0f);
	TestEqual(TEXT("Each damage effect removes exactly one health"), FireballAbilitySystem->GetNumericAttribute(URSHealthSet::GetHealthAttribute()), 1.0f);
	TestEqual(TEXT("Pending charge completion still accepts same-frame hits"), Fireball->GetFireballState(), ERSBossFireballState::ChargeCompletedPending);

	ApplyFireballTestDamage(FireballAbilitySystem, 50.0f);
	TestEqual(TEXT("The last same-frame damage reduces health to zero"), FireballAbilitySystem->GetNumericAttribute(URSHealthSet::GetHealthAttribute()), 0.0f);
	TestEqual(TEXT("Zero health moves the actor to cleanup before field transition"), Fireball->GetFireballState(), ERSBossFireballState::Cleanup);

	DestroyFireballTestWorld(TestWorld);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSBossFireballChargeTelegraphTest, "RS.Boss.FireballSpread.ChargeTelegraph", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSBossFireballChargeTelegraphTest::RunTest(const FString& Parameters)
{
	FWorldContext* WorldContext = nullptr;
	UWorld* TestWorld = CreateFireballTestWorld(WorldContext);
	TestNotNull(TEXT("Fireball telegraph test world"), TestWorld);
	if (!TestWorld)
	{
		return false;
	}

	FRSBossFireballDefinition FireballDefinition;
	ARSBossFireball* Fireball = SpawnFireball(TestWorld, FireballDefinition, true);
	TestNotNull(TEXT("Fireball actor"), Fireball);
	if (!Fireball)
	{
		DestroyFireballTestWorld(TestWorld);

		return false;
	}

	TestTrue(TEXT("Falling shows no ground display"), Fireball->GetTelegraphHandleForTest() == INDEX_NONE);

	Fireball->Tick(1.1f);
	TestEqual(TEXT("Finishing the fall starts charging"), Fireball->GetFireballState(), ERSBossFireballState::Charging);

	const int32 ChargeTelegraphHandle = Fireball->GetTelegraphHandleForTest();
	TestTrue(TEXT("Landing shows the fire field range as a warning"), ChargeTelegraphHandle != INDEX_NONE);
	UMaterialInstanceDynamic* ChargeMaterialInstance = FindVisibleTelegraphMaterialInstance(Fireball);
	TestNotNull(TEXT("The warning display belongs to the fireball actor"), ChargeMaterialInstance);
	if (ChargeMaterialInstance)
	{
		TestEqual(TEXT("The warning starts empty"), ChargeMaterialInstance->K2_GetScalarParameterValue(TEXT("Fill")), 0.0f);
		TestEqual(TEXT("The warning uses full opacity"), ChargeMaterialInstance->K2_GetScalarParameterValue(TEXT("Alpha")), 1.0f);
	}

	Fireball->Tick(1.5f);
	if (ChargeMaterialInstance)
	{
		TestEqual(TEXT("The warning fills with the charge progress"), ChargeMaterialInstance->K2_GetScalarParameterValue(TEXT("Fill")), 0.5f);
	}

	Fireball->BeginFireFieldForTest();
	TestEqual(TEXT("The fireball becomes a fire field"), Fireball->GetFireballState(), ERSBossFireballState::FireField);
	TestEqual(TEXT("The fire field keeps the handle the warning used"), Fireball->GetTelegraphHandleForTest(), ChargeTelegraphHandle);

	UMaterialInstanceDynamic* FieldMaterialInstance = FindVisibleTelegraphMaterialInstance(Fireball);
	TestNotNull(TEXT("The fire field keeps a visible range display"), FieldMaterialInstance);
	if (FieldMaterialInstance)
	{
		TestEqual(TEXT("The fire field display stops moving at a full fill"), FieldMaterialInstance->K2_GetScalarParameterValue(TEXT("Fill")), 1.0f);
		TestTrue(TEXT("The fire field display is dimmer than the warning"), FieldMaterialInstance->K2_GetScalarParameterValue(TEXT("Alpha")) < 1.0f);
	}

	Fireball->RequestCleanup();
	TestTrue(TEXT("Cleanup releases the range display"), Fireball->GetTelegraphHandleForTest() == INDEX_NONE);

	DestroyFireballTestWorld(TestWorld);

	return true;
}

#endif
