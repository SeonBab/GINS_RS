#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "RSAbilitySystemComponent.h"
#include "RSGameplayAbility_Interact.h"
#include "RSGameplayTags.h"

namespace
{
	/** 상호작용 어빌리티 테스트용 임시 월드를 만듭니다 */
	UWorld* CreateInteractionAbilityTestWorld()
	{
		const FName WorldName = MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("RSInteractionAbilityTestWorld"), EUniqueObjectNameOptions::GloballyUnique);
		FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
		UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false, WorldName, GetTransientPackage());
		TestWorld->AddToRoot();
		WorldContext.SetCurrentWorld(TestWorld);
		TestWorld->InitializeActorsForPlay(FURL());

		return TestWorld;
	}

	/** 테스트 월드가 남긴 WorldContext와 Root 참조를 함께 정리합니다 */
	void DestroyInteractionAbilityTestWorld(UWorld* TestWorld)
	{
		if (!TestWorld)
		{
			return;
		}

		TestWorld->RemoveFromRoot();
		GEngine->DestroyWorldContext(TestWorld);
		TestWorld->DestroyWorld(false);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSInteractionAbilityBlockingTest, "RS.Interaction.Ability.Blocking", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSInteractionAbilityBlockingTest::RunTest(const FString& Parameters)
{
	UWorld* TestWorld = CreateInteractionAbilityTestWorld();
	TestNotNull(TEXT("Interaction ability test world"), TestWorld);
	if (!TestWorld)
	{
		return false;
	}

	AActor* Owner = TestWorld->SpawnActor<AActor>();
	URSAbilitySystemComponent* AbilitySystemComponent = NewObject<URSAbilitySystemComponent>(Owner);
	AbilitySystemComponent->RegisterComponent();
	AbilitySystemComponent->InitAbilityActorInfo(Owner, Owner);

	const FGameplayAbilitySpecHandle InteractHandle = AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(URSGameplayAbility_Interact::StaticClass(), 1));

	// 선택된 대상이 없어도 활성화 자체는 성공하고 즉시 끝나야 잠긴 상태가 남지 않습니다
	TestTrue(TEXT("The interact ability activates even without a focused target"), AbilitySystemComponent->TryActivateAbility(InteractHandle));

	const FGameplayAbilitySpec* InteractSpec = AbilitySystemComponent->FindAbilitySpecFromHandle(InteractHandle);
	TestNotNull(TEXT("Granted interact ability spec"), InteractSpec);
	if (!InteractSpec)
	{
		DestroyInteractionAbilityTestWorld(TestWorld);

		return false;
	}

	TestFalse(TEXT("The interact ability does not stay active when nothing was interacted with"), InteractSpec->IsActive());

	// 행동 잠금 중에는 GAS가 활성화를 막아야 합니다
	AbilitySystemComponent->AddLooseGameplayTag(RSGameplayTags::State_Action_Locked);
	TestFalse(TEXT("A locked action blocks the interact ability"), AbilitySystemComponent->TryActivateAbility(InteractHandle));
	AbilitySystemComponent->RemoveLooseGameplayTag(RSGameplayTags::State_Action_Locked);

	// 사망 상태는 공용 기반 어빌리티가 막습니다
	AbilitySystemComponent->AddLooseGameplayTag(RSGameplayTags::State_Dead);
	TestFalse(TEXT("A dead player cannot interact"), AbilitySystemComponent->TryActivateAbility(InteractHandle));
	AbilitySystemComponent->RemoveLooseGameplayTag(RSGameplayTags::State_Dead);

	TestTrue(TEXT("The interact ability activates again once the locks are gone"), AbilitySystemComponent->TryActivateAbility(InteractHandle));

	DestroyInteractionAbilityTestWorld(TestWorld);

	return true;
}

#endif
