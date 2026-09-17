#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Abilities/RSBaseGameplayAbility.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "RSAbilitySystemComponent.h"
#include "RSGameplayTags.h"

namespace
{
	/** 입력 반복 테스트용 임시 월드를 만들고 Ability 부여가 가능한 권한 상태로 초기화합니다 */
	UWorld* CreateAbilityInputTestWorld()
	{
		const FName WorldName = MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("RSAbilityInputRepeatTestWorld"), EUniqueObjectNameOptions::GloballyUnique);
		FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
		UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false, WorldName, GetTransientPackage());
		TestWorld->AddToRoot();
		WorldContext.SetCurrentWorld(TestWorld);
		TestWorld->InitializeActorsForPlay(FURL());

		return TestWorld;
	}

	/** 테스트 월드가 남긴 WorldContext와 Root 참조를 함께 정리합니다 */
	void DestroyAbilityInputTestWorld(UWorld* TestWorld)
	{
		if (!TestWorld)
		{
			return;
		}

		TestWorld->RemoveFromRoot();
		GEngine->DestroyWorldContext(TestWorld);
		TestWorld->DestroyWorld(false);
	}

	/** 테스트 ASC에 입력 태그를 가진 Ability를 부여합니다 */
	void GiveInputAbility(URSAbilitySystemComponent& AbilitySystemComponent, UClass* AbilityClass, const FGameplayTag& InputTag)
	{
		FGameplayAbilitySpec AbilitySpec(AbilityClass, 1);
		AbilitySpec.GetDynamicSpecSourceTags().AddTag(InputTag);

		AbilitySystemComponent.GiveAbility(AbilitySpec);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSAbilityInputHeldRepeatTest, "RS.Input.HeldRepeat.BasicAttack", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSAbilityInputHeldRepeatTest::RunTest(const FString& Parameters)
{
	UClass* Step01Class = LoadClass<URSBaseGameplayAbility>(nullptr, TEXT("/Game/AbilitySystem/Abilities/Player/GA_BasicAttack_Step01.GA_BasicAttack_Step01_C"));
	UClass* Step02Class = LoadClass<URSBaseGameplayAbility>(nullptr, TEXT("/Game/AbilitySystem/Abilities/Player/GA_BasicAttack_Step02.GA_BasicAttack_Step02_C"));
	UClass* SkillClass = LoadClass<URSBaseGameplayAbility>(nullptr, TEXT("/Game/AbilitySystem/Abilities/Player/GA_Skill1.GA_Skill1_C"));
	TestNotNull(TEXT("Basic attack step 01 class"), Step01Class);
	TestNotNull(TEXT("Basic attack step 02 class"), Step02Class);
	TestNotNull(TEXT("Skill slot 01 class"), SkillClass);
	if (!Step01Class || !Step02Class || !SkillClass)
	{
		return false;
	}

	TestTrue(TEXT("Both combo steps repeat while the input is held"),
		Step01Class->GetDefaultObject<URSBaseGameplayAbility>()->ShouldRepeatWhileInputHeld() && Step02Class->GetDefaultObject<URSBaseGameplayAbility>()->ShouldRepeatWhileInputHeld());
	TestFalse(TEXT("A skill does not repeat while its input is held"), SkillClass->GetDefaultObject<URSBaseGameplayAbility>()->ShouldRepeatWhileInputHeld());

	UWorld* TestWorld = CreateAbilityInputTestWorld();
	TestNotNull(TEXT("Ability input test world"), TestWorld);
	if (!TestWorld)
	{
		return false;
	}

	AActor* Owner = TestWorld->SpawnActor<AActor>();
	URSAbilitySystemComponent* AbilitySystemComponent = NewObject<URSAbilitySystemComponent>(Owner);
	AbilitySystemComponent->RegisterComponent();
	AbilitySystemComponent->InitAbilityActorInfo(Owner, Owner);

	GiveInputAbility(*AbilitySystemComponent, Step01Class, RSGameplayTags::InputTag_Ability_BasicAttack);
	GiveInputAbility(*AbilitySystemComponent, Step02Class, RSGameplayTags::InputTag_Ability_BasicAttack);
	GiveInputAbility(*AbilitySystemComponent, SkillClass, RSGameplayTags::InputTag_Ability_SkillSlot01);

	TestFalse(TEXT("No input is repeated before anything is held"), AbilitySystemComponent->FindHeldRepeatInputTagForTest().IsValid());

	AbilitySystemComponent->AbilityInputTagPressed(RSGameplayTags::InputTag_Ability_SkillSlot01);
	TestFalse(TEXT("A held skill input is not repeated"), AbilitySystemComponent->FindHeldRepeatInputTagForTest().IsValid());

	AbilitySystemComponent->AbilityInputTagPressed(RSGameplayTags::InputTag_Ability_BasicAttack);
	TestEqual(TEXT("A held basic attack input is repeated"), AbilitySystemComponent->FindHeldRepeatInputTagForTest(), RSGameplayTags::InputTag_Ability_BasicAttack.GetTag());

	AbilitySystemComponent->AbilityInputTagReleased(RSGameplayTags::InputTag_Ability_BasicAttack);
	TestFalse(TEXT("A released basic attack input stops repeating"), AbilitySystemComponent->FindHeldRepeatInputTagForTest().IsValid());

	AbilitySystemComponent->AbilityInputTagPressed(RSGameplayTags::InputTag_Ability_BasicAttack);
	AbilitySystemComponent->ClearAbilityInput();
	TestFalse(TEXT("Clearing input stops repeating"), AbilitySystemComponent->FindHeldRepeatInputTagForTest().IsValid());

	DestroyAbilityInputTestWorld(TestWorld);

	return true;
}

#endif
