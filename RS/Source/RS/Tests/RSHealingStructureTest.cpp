#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AbilitySystemComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
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
	constexpr float ChargedHealDelaySeconds = 1.5f;

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
		FFloatProperty* ChargedHealDelayProperty = FindFProperty<FFloatProperty>(ARSHealingStructure::StaticClass(), TEXT("ChargedHealDelaySeconds"));
		FClassProperty* HealEffectClassProperty = FindFProperty<FClassProperty>(ARSHealingStructure::StaticClass(), TEXT("HealEffectClass"));

		USphereComponent* TriggerArea = Structure ? Structure->FindComponentByClass<USphereComponent>() : nullptr;
		if (!Structure || !StartChargedProperty || !RechargeSecondsProperty || !HealAmountProperty || !ChargedHealDelayProperty || !HealEffectClassProperty || !TriggerArea)
		{
			return false;
		}

		StartChargedProperty->SetPropertyValue_InContainer(Structure, bStartCharged);
		RechargeSecondsProperty->SetPropertyValue_InContainer(Structure, RechargeSeconds);
		HealAmountProperty->SetPropertyValue_InContainer(Structure, HealAmount);
		ChargedHealDelayProperty->SetPropertyValue_InContainer(Structure, ChargedHealDelaySeconds);
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

		// FinishSpawning이 컴포넌트를 등록해야 감지 영역이 Overlap을 잡습니다
		Structure->FinishSpawning(SpawnTransform);

		// 임시 월드에서는 FinishSpawning이 BeginPlay까지 디스패치하지 않아 시작 충전 상태가 적용되지 않습니다
		if (!Structure->HasActorBegunPlay())
		{
			Structure->DispatchBeginPlay();
		}

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

	UStaticMeshComponent* ChargedMesh = Structure->FindComponentByClass<UStaticMeshComponent>();
	TestNotNull(TEXT("Charged mesh"), ChargedMesh);
	if (!ChargedMesh)
	{
		RSHealingStructureTest::DestroyTestWorld(TestWorld);

		return false;
	}

	// 표시는 바로 보이고 회복만 지연되므로 어떤 경로로 소모되든 Mesh가 그 시간만큼은 화면에 남습니다
	TestTrue(TEXT("Charging shows the charged mesh right away"), ChargedMesh->IsVisible());
	TestFalse(TEXT("Charging does not open healing in the same frame"), Structure->IsReadyToHeal());
	TestEqual(TEXT("Charging does not heal in the same frame"), HealthComp->GetHealth(), DamagedHealth);
	TestTrue(TEXT("Charging schedules the moment healing opens"), Structure->IsChargedHealDelayActiveForTest());
	TestEqual(TEXT("Healing opens after the configured delay"), Structure->GetChargedHealDelayRemainingForTest(), RSHealingStructureTest::ChargedHealDelaySeconds);

	Structure->CompleteChargedHealDelayForTest();

	TestEqual(TEXT("Opened healing reaches the target already inside"), HealthComp->GetHealth(), DamagedHealth + RSHealingStructureTest::HealAmount);
	TestFalse(TEXT("Successful healing closes healing again"), Structure->IsReadyToHeal());
	TestFalse(TEXT("Successful healing hides the charged mesh"), ChargedMesh->IsVisible());
	TestTrue(TEXT("Consumed charge schedules a recharge"), Structure->IsRechargeTimerActiveForTest());
	TestEqual(TEXT("Recharge waits the configured duration"), Structure->GetRechargeRemainingForTest(), RSHealingStructureTest::RechargeSeconds);

	const float HealthAfterFirstHeal = HealthComp->GetHealth();

	// 재충전이 끝난 시점에 영역 안에 있으면 다시 들어오지 않아도 회복하며 같은 지연을 거칩니다
	Structure->CompleteRechargeForTest();

	TestTrue(TEXT("Recharge completion shows the charged mesh again"), ChargedMesh->IsVisible());
	TestFalse(TEXT("Recharge completion does not open healing immediately"), Structure->IsReadyToHeal());
	TestEqual(TEXT("Recharge completion does not heal before the delay"), HealthComp->GetHealth(), HealthAfterFirstHeal);

	Structure->CompleteChargedHealDelayForTest();

	TestEqual(TEXT("Recharge completion heals the target still inside"), HealthComp->GetHealth(), HealthAfterFirstHeal + RSHealingStructureTest::HealAmount);
	TestFalse(TEXT("Immediate healing closes healing again"), Structure->IsReadyToHeal());

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

	// 체력이 가득 차 있어도 충전을 소모해야 재충전 타이머가 계속 돌고 그 자리에서 다쳐도 다음 주기에 회복됩니다
	ARSHealingStructure* Structure = RSHealingStructureTest::SpawnStructure(TestWorld, FVector::ZeroVector, true);
	TestNotNull(TEXT("Healing structure"), Structure);
	if (!Structure)
	{
		RSHealingStructureTest::DestroyTestWorld(TestWorld);

		return false;
	}

	Structure->CompleteChargedHealDelayForTest();

	TestFalse(TEXT("A full health target still consumes the charge"), Structure->IsReadyToHeal());
	TestTrue(TEXT("Healing a full health target keeps the recharge cycle running"), Structure->IsRechargeTimerActiveForTest());
	TestEqual(TEXT("Healing does not push health above the maximum"), HealthComp->GetHealth(), HealthComp->GetMaxHealth());

	// 서 있는 동안 피해를 입어도 다음 재충전이 회복시킵니다
	constexpr float DamageAmount = 40.0f;
	RSHealingStructureTest::ApplyHealingStructureTestDamage(PlayerCharacter, DamageAmount);
	const float DamagedHealth = HealthComp->GetHealth();

	Structure->CompleteRechargeForTest();
	Structure->CompleteChargedHealDelayForTest();

	TestEqual(TEXT("The next recharge heals a target hurt while standing inside"), HealthComp->GetHealth(), DamagedHealth + RSHealingStructureTest::HealAmount);

	// 사망한 대상을 회복시키면 체력이 0을 넘는데 사망 상태는 그대로라 회복 대상에서 제외합니다
	RSHealingStructureTest::ApplyHealingStructureTestDamage(PlayerCharacter, HealthComp->GetMaxHealth());
	TestTrue(TEXT("Lethal damage marks the target dead"), HealthComp->IsDead());

	Structure->CompleteRechargeForTest();
	Structure->CompleteChargedHealDelayForTest();

	TestTrue(TEXT("A dead target does not consume the charge"), Structure->IsReadyToHeal());
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

	TestFalse(TEXT("Starting uncharged leaves the structure unusable"), Structure->IsReadyToHeal());
	TestFalse(TEXT("Starting uncharged schedules no delayed heal"), Structure->IsChargedHealDelayActiveForTest());
	TestEqual(TEXT("Starting uncharged heals nobody at BeginPlay"), HealthComp->GetHealth(), DamagedHealth);
	TestTrue(TEXT("Starting uncharged schedules the first recharge"), Structure->IsRechargeTimerActiveForTest());
	TestEqual(TEXT("The first recharge waits the configured duration"), Structure->GetRechargeRemainingForTest(), RSHealingStructureTest::RechargeSeconds);

	Structure->CompleteRechargeForTest();
	Structure->CompleteChargedHealDelayForTest();

	TestEqual(TEXT("The first recharge heals the waiting target"), HealthComp->GetHealth(), DamagedHealth + RSHealingStructureTest::HealAmount);

	RSHealingStructureTest::DestroyTestWorld(TestWorld);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
