#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AbilitySystemComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "RSAbilitySystemComponent.h"
#include "RSHealthComponent.h"
#include "RSHealthInitializationTestTypes.h"
#include "RSHealthSet.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSHealthInitializationTest, "RS.Combat.Health.Initialization", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSHealthRuntimeInitializationTest, "RS.Combat.Health.RuntimeInitialization", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSHealthInitializationTest::RunTest(const FString& Parameters)
{
	const FName WorldName = MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("RSHealthInitializationTestWorld"), EUniqueObjectNameOptions::GloballyUnique);
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false, WorldName, GetTransientPackage());
	TestWorld->AddToRoot();
	WorldContext.SetCurrentWorld(TestWorld);
	TestWorld->InitializeActorsForPlay(FURL());

	AActor* Owner = TestWorld->SpawnActor<AActor>();
	URSAbilitySystemComponent* AbilitySystemComp = NewObject<URSAbilitySystemComponent>(Owner);
	URSHealthComponent* HealthComp = NewObject<URSHealthComponent>(Owner);

	// 기획자가 Blueprint에서 설정하는 값이므로 EditDefaultsOnly 속성을 직접 찾아 테스트 값으로 바꿉니다
	FFloatProperty* InitialMaxHealthProperty = FindFProperty<FFloatProperty>(URSHealthComponent::StaticClass(), TEXT("InitialMaxHealth"));
	TestNotNull(TEXT("InitialMaxHealth property"), InitialMaxHealthProperty);
	if (!Owner || !AbilitySystemComp || !HealthComp || !InitialMaxHealthProperty)
	{
		TestWorld->RemoveFromRoot();
		GEngine->DestroyWorldContext(TestWorld);
		TestWorld->DestroyWorld(false);

		return false;
	}

	constexpr float ConfiguredMaxHealth = 5000.0f;
	InitialMaxHealthProperty->SetPropertyValue_InContainer(HealthComp, ConfiguredMaxHealth);

	AbilitySystemComp->RegisterComponent();
	AbilitySystemComp->InitAbilityActorInfo(Owner, Owner);
	AbilitySystemComp->AddAttributeSetSubobject(NewObject<URSHealthSet>(Owner));
	HealthComp->RegisterComponent();

	// 사용자 인터페이스는 초기 동기화 브로드캐스트만 보고 체력을 그리므로 그 값이 기획 값인지 확인합니다
	URSHealthInitializationTestListener* Listener = NewObject<URSHealthInitializationTestListener>();
	Listener->ListenTo(HealthComp);

	HealthComp->InitializeWithAbilitySystem(AbilitySystemComp);

	TestEqual(TEXT("Configured max health replaces the HealthSet default"), AbilitySystemComp->GetNumericAttribute(URSHealthSet::GetMaxHealthAttribute()), ConfiguredMaxHealth);
	TestEqual(TEXT("Health starts full at the configured max health"), AbilitySystemComp->GetNumericAttribute(URSHealthSet::GetHealthAttribute()), ConfiguredMaxHealth);
	TestEqual(TEXT("Initial max health broadcast carries the configured value"), Listener->MaxHealthValues.Num() > 0 ? Listener->MaxHealthValues[0] : 0.0f, ConfiguredMaxHealth);
	TestEqual(TEXT("Initial health broadcast carries the configured value"), Listener->HealthValues.Num() > 0 ? Listener->HealthValues[0] : 0.0f, ConfiguredMaxHealth);
	TestFalse(TEXT("Configured max health does not start death"), HealthComp->IsDead());

	TestWorld->RemoveFromRoot();
	GEngine->DestroyWorldContext(TestWorld);
	TestWorld->DestroyWorld(false);

	return true;
}

bool FRSHealthRuntimeInitializationTest::RunTest(const FString& Parameters)
{
	const FName WorldName = MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("RSHealthRuntimeInitializationTestWorld"), EUniqueObjectNameOptions::GloballyUnique);
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false, WorldName, GetTransientPackage());
	TestWorld->AddToRoot();
	WorldContext.SetCurrentWorld(TestWorld);
	TestWorld->InitializeActorsForPlay(FURL());

	AActor* Owner = TestWorld->SpawnActor<AActor>();
	URSAbilitySystemComponent* AbilitySystemComp = NewObject<URSAbilitySystemComponent>(Owner);
	URSHealthComponent* HealthComp = NewObject<URSHealthComponent>(Owner);
	if (!Owner || !AbilitySystemComp || !HealthComp)
	{
		TestWorld->RemoveFromRoot();
		GEngine->DestroyWorldContext(TestWorld);
		TestWorld->DestroyWorld(false);

		return false;
	}

	AbilitySystemComp->RegisterComponent();
	AbilitySystemComp->InitAbilityActorInfo(Owner, Owner);
	AbilitySystemComp->AddAttributeSetSubobject(NewObject<URSHealthSet>(Owner));
	HealthComp->RegisterComponent();

	// 화염구처럼 필요한 타격 수가 생성 시점에 정해지는 대상은 Blueprint 기본값 대신 런타임 값을 사용합니다
	constexpr float RuntimeMaxHealth = 3.0f;
	HealthComp->SetInitialMaxHealth(RuntimeMaxHealth);
	HealthComp->InitializeWithAbilitySystem(AbilitySystemComp);

	TestEqual(TEXT("Runtime max health replaces the configured default"), HealthComp->GetMaxHealth(), RuntimeMaxHealth);
	TestEqual(TEXT("Runtime health starts full"), HealthComp->GetHealth(), RuntimeMaxHealth);

	TestWorld->RemoveFromRoot();
	GEngine->DestroyWorldContext(TestWorld);
	TestWorld->DestroyWorld(false);

	return true;
}

#endif
