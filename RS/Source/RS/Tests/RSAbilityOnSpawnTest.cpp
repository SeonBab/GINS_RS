#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "RSAbilityOnSpawnTestTypes.h"
#include "RSAbilitySystemComponent.h"

namespace
{
	/** OnSpawn 활성화 테스트용 임시 월드를 만들고 Ability 부여가 가능한 권한 상태로 초기화합니다 */
	UWorld* CreateAbilityOnSpawnTestWorld()
	{
		const FName WorldName = MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("RSAbilityOnSpawnTestWorld"), EUniqueObjectNameOptions::GloballyUnique);
		FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
		UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false, WorldName, GetTransientPackage());
		TestWorld->AddToRoot();
		WorldContext.SetCurrentWorld(TestWorld);
		TestWorld->InitializeActorsForPlay(FURL());

		return TestWorld;
	}

	/** 테스트 월드가 남긴 WorldContext와 Root 참조를 함께 정리합니다 */
	void DestroyAbilityOnSpawnTestWorld(UWorld* TestWorld)
	{
		if (!TestWorld)
		{
			return;
		}

		TestWorld->RemoveFromRoot();
		GEngine->DestroyWorldContext(TestWorld);
		TestWorld->DestroyWorld(false);
	}

	/** 부여한 Ability가 실행 중인지 반환합니다 */
	bool IsAbilityActive(const URSAbilitySystemComponent& AbilitySystemComponent, const FGameplayAbilitySpecHandle& Handle)
	{
		const FGameplayAbilitySpec* AbilitySpec = AbilitySystemComponent.FindAbilitySpecFromHandle(Handle);

		return AbilitySpec && AbilitySpec->IsActive();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSAbilityOnSpawnActivationTest, "RS.Ability.ActivationPolicy.OnSpawn", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSAbilityOnSpawnActivationTest::RunTest(const FString& Parameters)
{
	UWorld* TestWorld = CreateAbilityOnSpawnTestWorld();
	TestNotNull(TEXT("Ability on spawn test world"), TestWorld);
	if (!TestWorld)
	{
		return false;
	}

	AActor* Owner = TestWorld->SpawnActor<AActor>();
	URSAbilitySystemComponent* AbilitySystemComponent = NewObject<URSAbilitySystemComponent>(Owner);
	AbilitySystemComponent->RegisterComponent();
	AbilitySystemComponent->InitAbilityActorInfo(Owner, Owner);

	const FGameplayAbilitySpecHandle OnSpawnHandle = AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(URSAbilityOnSpawnTestAbility::StaticClass(), 1));
	const FGameplayAbilitySpecHandle ManualHandle = AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(URSAbilityOnSpawnTestManualAbility::StaticClass(), 1));

	TestFalse(TEXT("An OnSpawn ability is not active merely by being granted"), IsAbilityActive(*AbilitySystemComponent, OnSpawnHandle));

	AbilitySystemComponent->TryActivateAbilitiesOnSpawn();

	TestTrue(TEXT("An OnSpawn ability activates without input"), IsAbilityActive(*AbilitySystemComponent, OnSpawnHandle));
	TestFalse(TEXT("A None policy ability stays inactive"), IsAbilityActive(*AbilitySystemComponent, ManualHandle));

	// 두 진입 경로에서 호출되어도 실행 중인 어빌리티를 다시 켜지 않는지 확인합니다
	const FGameplayAbilitySpec* OnSpawnSpec = AbilitySystemComponent->FindAbilitySpecFromHandle(OnSpawnHandle);
	TestNotNull(TEXT("Granted OnSpawn ability spec"), OnSpawnSpec);
	if (!OnSpawnSpec)
	{
		DestroyAbilityOnSpawnTestWorld(TestWorld);

		return false;
	}

	const int32 ActiveCountAfterFirstCall = OnSpawnSpec->GetAbilityInstances().Num();

	AbilitySystemComponent->TryActivateAbilitiesOnSpawn();

	TestEqual(TEXT("Calling the OnSpawn path twice does not activate the ability again"), OnSpawnSpec->GetAbilityInstances().Num(), ActiveCountAfterFirstCall);

	DestroyAbilityOnSpawnTestWorld(TestWorld);

	return true;
}

#endif
