#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AbilitySystemComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "RSHealingStructure.h"
#include "RSHealingStructureTestTypes.h"
#include "RSHealthComponent.h"
#include "RSHealthSet.h"
#include "RSPlayerCharacter.h"
#include "RSPlayerState.h"
#include "UObject/UnrealType.h"

namespace RSHealingStructureTest
{
	constexpr float TriggerRadius = 300.0f;
	constexpr float HealAmount = 25.0f;
	constexpr float RechargeSeconds = 12.0f;

	/** Overlap과 Timer를 함께 검증할 Play 상태 임시 월드를 만듭니다 */
	UWorld* CreateTestWorld()
	{
		const FName WorldName = MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("RSHealingStructureTestWorld"), EUniqueObjectNameOptions::GloballyUnique);
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

	/** 실제 Player Class와 PlayerState ASC를 함께 구성해 구조물의 타입·사망 필터를 통과시킵니다 */
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

	/** 회복이 필요한 상태를 만들기 위해 대상의 체력을 줄입니다 */
	void ApplyHealingStructureTestDamage(ARSPlayerCharacter* PlayerCharacter, float DamageAmount)
	{
		UAbilitySystemComponent* TargetAbilitySystemComp = PlayerCharacter ? PlayerCharacter->GetAbilitySystemComponent() : nullptr;
		if (!TargetAbilitySystemComp)
		{
			return;
		}

		UGameplayEffect* DamageEffect = NewObject<UGameplayEffect>(GetTransientPackage(), TEXT("RSHealingStructureTestDamageEffect"));
		DamageEffect->DurationPolicy = EGameplayEffectDurationType::Instant;

		FGameplayModifierInfo DamageModifier;
		DamageModifier.Attribute = URSHealthSet::GetDamageAttribute();
		DamageModifier.ModifierOp = EGameplayModOp::Additive;
		DamageModifier.ModifierMagnitude = FScalableFloat(DamageAmount);
		DamageEffect->Modifiers.Add(DamageModifier);

		FGameplayEffectContextHandle EffectContext = TargetAbilitySystemComp->MakeEffectContext();
		FGameplayEffectSpec DamageSpec(DamageEffect, EffectContext, 1.0f);
		TargetAbilitySystemComp->ApplyGameplayEffectSpecToSelf(DamageSpec);
	}

	/**
	 * 기획자가 Class Defaults에서 설정하는 값이므로 EditAnywhere 속성을 직접 찾아 테스트 값으로 바꿉니다
	 * 감지 반지름은 Blueprint가 컴포넌트에 설정하는 값이라 같은 시점에 함께 넣습니다
	 */
	bool ConfigureStructure(ARSHealingStructure* Structure, bool bStartCharged)
	{
		FBoolProperty* StartChargedProperty = FindFProperty<FBoolProperty>(ARSHealingStructure::StaticClass(), TEXT("bStartCharged"));
		FFloatProperty* RechargeSecondsProperty = FindFProperty<FFloatProperty>(ARSHealingStructure::StaticClass(), TEXT("RechargeSeconds"));
		FFloatProperty* HealAmountProperty = FindFProperty<FFloatProperty>(ARSHealingStructure::StaticClass(), TEXT("HealAmount"));
		FClassProperty* HealEffectClassProperty = FindFProperty<FClassProperty>(ARSHealingStructure::StaticClass(), TEXT("HealEffectClass"));

		USphereComponent* TriggerArea = Structure ? Structure->FindComponentByClass<USphereComponent>() : nullptr;
		if (!Structure || !StartChargedProperty || !RechargeSecondsProperty || !HealAmountProperty || !HealEffectClassProperty || !TriggerArea)
		{
			return false;
		}

		StartChargedProperty->SetPropertyValue_InContainer(Structure, bStartCharged);
		RechargeSecondsProperty->SetPropertyValue_InContainer(Structure, RechargeSeconds);
		HealAmountProperty->SetPropertyValue_InContainer(Structure, HealAmount);
		HealEffectClassProperty->SetPropertyValue_InContainer(Structure, URSHealingStructureTestHealEffect::StaticClass());

		TriggerArea->SetSphereRadius(TriggerRadius);

		return true;
	}

	/** BeginPlay가 시작 충전 상태를 읽기 전에 기획 값을 넣어야 하므로 Deferred로 생성합니다 */
	ARSHealingStructure* SpawnStructure(UWorld* TestWorld, const FVector& Location, bool bStartCharged)
	{
		if (!TestWorld)
		{
			return nullptr;
		}

		const FTransform SpawnTransform(Location);
		ARSHealingStructure* Structure = TestWorld->SpawnActorDeferred<ARSHealingStructure>(ARSHealingStructure::StaticClass(), SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!ConfigureStructure(Structure, bStartCharged))
		{
			return nullptr;
		}

		Structure->FinishSpawning(SpawnTransform);

		return Structure;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSHealingStructureChargeTest, "RS.World.HealingStructure.Charge", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSHealingStructureTargetFilterTest, "RS.World.HealingStructure.TargetFilter", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSHealingStructureStartUnchargedTest, "RS.World.HealingStructure.StartUncharged", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSHealingStructureChargeTest::RunTest(const FString& Parameters)
{
	UWorld* TestWorld = RSHealingStructureTest::CreateTestWorld();
	ARSPlayerCharacter* PlayerCharacter = RSHealingStructureTest::SpawnPlayer(TestWorld, FVector::ZeroVector);
	if (!PlayerCharacter)
	{
		RSHealingStructureTest::DestroyTestWorld(TestWorld);

		return false;
	}

	URSHealthComponent* HealthComp = PlayerCharacter->GetHealthComponent();
	TestNotNull(TEXT("Player health component"), HealthComp);
	if (!HealthComp)
	{
		RSHealingStructureTest::DestroyTestWorld(TestWorld);

		return false;
	}

	const float MaxHealth = HealthComp->GetMaxHealth();
	constexpr float DamageAmount = 60.0f;
	RSHealingStructureTest::ApplyHealingStructureTestDamage(PlayerCharacter, DamageAmount);

	const float DamagedHealth = HealthComp->GetHealth();
	TestEqual(TEXT("Damage reduced health before the structure exists"), DamagedHealth, MaxHealth - DamageAmount);

	// 충전된 구조물이 다친 플레이어를 이미 품고 있는 상태에서 시작합니다
	ARSHealingStructure* Structure = RSHealingStructureTest::SpawnStructure(TestWorld, FVector::ZeroVector, true);
	TestNotNull(TEXT("Healing structure"), Structure);
	if (!Structure)
	{
		RSHealingStructureTest::DestroyTestWorld(TestWorld);

		return false;
	}

	TestEqual(TEXT("Charged structure heals the target already inside"), HealthComp->GetHealth(), DamagedHealth + RSHealingStructureTest::HealAmount);
	TestFalse(TEXT("Successful healing consumes the charge"), Structure->IsCharged());
	TestTrue(TEXT("Consumed charge schedules a recharge"), Structure->IsRechargeTimerActiveForTest());
	TestEqual(TEXT("Recharge waits the configured duration"), Structure->GetRechargeRemainingForTest(), RSHealingStructureTest::RechargeSeconds);

	const float HealthAfterFirstHeal = HealthComp->GetHealth();

	// 재충전이 끝난 시점에 영역 안에 있으면 다시 들어오지 않아도 회복합니다
	Structure->CompleteRechargeForTest();

	TestEqual(TEXT("Recharge completion heals the target still inside"), HealthComp->GetHealth(), HealthAfterFirstHeal + RSHealingStructureTest::HealAmount);
	TestFalse(TEXT("Immediate healing consumes the charge again"), Structure->IsCharged());

	RSHealingStructureTest::DestroyTestWorld(TestWorld);

	return true;
}

bool FRSHealingStructureTargetFilterTest::RunTest(const FString& Parameters)
{
	UWorld* TestWorld = RSHealingStructureTest::CreateTestWorld();
	ARSPlayerCharacter* PlayerCharacter = RSHealingStructureTest::SpawnPlayer(TestWorld, FVector::ZeroVector);
	if (!PlayerCharacter)
	{
		RSHealingStructureTest::DestroyTestWorld(TestWorld);

		return false;
	}

	URSHealthComponent* HealthComp = PlayerCharacter->GetHealthComponent();
	TestNotNull(TEXT("Player health component"), HealthComp);
	if (!HealthComp)
	{
		RSHealingStructureTest::DestroyTestWorld(TestWorld);

		return false;
	}

	// 체력이 가득 찬 대상만 있으면 충전을 쓰지 않아야 다친 순간을 위해 남아 있습니다
	ARSHealingStructure* Structure = RSHealingStructureTest::SpawnStructure(TestWorld, FVector::ZeroVector, true);
	TestNotNull(TEXT("Healing structure"), Structure);
	if (!Structure)
	{
		RSHealingStructureTest::DestroyTestWorld(TestWorld);

		return false;
	}

	TestTrue(TEXT("A full health target does not consume the charge"), Structure->IsCharged());
	TestFalse(TEXT("No recharge is scheduled while the charge is kept"), Structure->IsRechargeTimerActiveForTest());
	TestEqual(TEXT("A full health target is not healed above the maximum"), HealthComp->GetHealth(), HealthComp->GetMaxHealth());

	// 사망한 대상은 회복 대상이 아니므로 충전이 그대로 남습니다
	RSHealingStructureTest::ApplyHealingStructureTestDamage(PlayerCharacter, HealthComp->GetMaxHealth());
	TestTrue(TEXT("Lethal damage marks the target dead"), HealthComp->IsDead());

	Structure->CompleteRechargeForTest();

	TestTrue(TEXT("A dead target does not consume the charge"), Structure->IsCharged());
	TestEqual(TEXT("A dead target is not healed"), HealthComp->GetHealth(), 0.0f);

	RSHealingStructureTest::DestroyTestWorld(TestWorld);

	return true;
}

bool FRSHealingStructureStartUnchargedTest::RunTest(const FString& Parameters)
{
	UWorld* TestWorld = RSHealingStructureTest::CreateTestWorld();
	ARSPlayerCharacter* PlayerCharacter = RSHealingStructureTest::SpawnPlayer(TestWorld, FVector::ZeroVector);
	if (!PlayerCharacter)
	{
		RSHealingStructureTest::DestroyTestWorld(TestWorld);

		return false;
	}

	URSHealthComponent* HealthComp = PlayerCharacter->GetHealthComponent();
	TestNotNull(TEXT("Player health component"), HealthComp);
	if (!HealthComp)
	{
		RSHealingStructureTest::DestroyTestWorld(TestWorld);

		return false;
	}

	constexpr float DamageAmount = 60.0f;
	RSHealingStructureTest::ApplyHealingStructureTestDamage(PlayerCharacter, DamageAmount);
	const float DamagedHealth = HealthComp->GetHealth();

	ARSHealingStructure* Structure = RSHealingStructureTest::SpawnStructure(TestWorld, FVector::ZeroVector, false);
	TestNotNull(TEXT("Healing structure"), Structure);
	if (!Structure)
	{
		RSHealingStructureTest::DestroyTestWorld(TestWorld);

		return false;
	}

	TestFalse(TEXT("Starting uncharged leaves the structure unusable"), Structure->IsCharged());
	TestEqual(TEXT("Starting uncharged heals nobody at BeginPlay"), HealthComp->GetHealth(), DamagedHealth);
	TestTrue(TEXT("Starting uncharged schedules the first recharge"), Structure->IsRechargeTimerActiveForTest());
	TestEqual(TEXT("The first recharge waits the configured duration"), Structure->GetRechargeRemainingForTest(), RSHealingStructureTest::RechargeSeconds);

	Structure->CompleteRechargeForTest();

	TestEqual(TEXT("The first recharge heals the waiting target"), HealthComp->GetHealth(), DamagedHealth + RSHealingStructureTest::HealAmount);

	RSHealingStructureTest::DestroyTestWorld(TestWorld);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
