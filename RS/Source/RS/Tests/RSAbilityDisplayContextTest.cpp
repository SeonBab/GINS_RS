#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Abilities/RSBaseGameplayAbility.h"
#include "Abilities/Player/RSGameplayAbility_Dash.h"
#include "Abilities/Player/RSGameplayAbility_GetUpQuick.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "RSAbilityDefinition.h"
#include "RSAbilitySystemComponent.h"
#include "RSPlayerAbilityViewModel.h"
#include "RSGameplayTags.h"

namespace
{
	/** 표시 Resolver 테스트용 임시 월드를 만들고 Ability 부여가 가능한 권한 상태로 초기화합니다 */
	UWorld* CreateAbilityDisplayTestWorld()
	{
		const FName WorldName = MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("RSAbilityDisplayTestWorld"), EUniqueObjectNameOptions::GloballyUnique);
		FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
		UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false, WorldName, GetTransientPackage());
		TestWorld->AddToRoot();
		WorldContext.SetCurrentWorld(TestWorld);
		TestWorld->InitializeActorsForPlay(FURL());

		return TestWorld;
	}

	/** 테스트 월드가 남긴 WorldContext와 Root 참조를 함께 정리합니다 */
	void DestroyAbilityDisplayTestWorld(UWorld* TestWorld)
	{
		if (!TestWorld)
		{
			return;
		}

		TestWorld->RemoveFromRoot();
		GEngine->DestroyWorldContext(TestWorld);
		TestWorld->DestroyWorld(false);
	}

	/** 테스트 ASC에 입력 태그가 같은 Ability를 부여합니다 */
	FGameplayAbilitySpecHandle GiveDisplayAbility(URSAbilitySystemComponent& AbilitySystemComponent, UClass* AbilityClass, const FGameplayTag& InputTag)
	{
		FGameplayAbilitySpec AbilitySpec(AbilityClass, 1);
		AbilitySpec.GetDynamicSpecSourceTags().AddTag(InputTag);

		return AbilitySystemComponent.GiveAbility(AbilitySpec);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSAbilityDisplayContextQuickGetUpTest, "RS.UI.AbilityDisplayContext.QuickGetUp", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSAbilityDisplayContextQuickGetUpTest::RunTest(const FString& Parameters)
{
	const URSGameplayAbility_Dash* DashAbility = GetDefault<URSGameplayAbility_Dash>();
	const URSGameplayAbility_GetUpQuick* QuickGetUpAbility = GetDefault<URSGameplayAbility_GetUpQuick>();
	const FGameplayTagContainer& DisplayContextTags = URSAbilitySystemComponent::GetDisplayContextTags();

	TestTrue(TEXT("Quick get-up availability is a display context"), DisplayContextTags.HasTagExact(RSGameplayTags::State_Recovery_QuickGetUpAvailable));
	TestFalse(TEXT("Downed no longer selects the displayed ability directly"), DisplayContextTags.HasTagExact(RSGameplayTags::State_CrowdControl_Downed));

	FGameplayTagContainer OwnedTags;
	TestTrue(TEXT("Dash represents Space before quick get-up is available"), DashAbility->SatisfiesDisplayContext(OwnedTags, DisplayContextTags));
	TestFalse(TEXT("Quick get-up does not represent Space before it is available"), QuickGetUpAbility->SatisfiesDisplayContext(OwnedTags, DisplayContextTags));

	OwnedTags.AddTag(RSGameplayTags::State_Recovery_QuickGetUpAvailable);
	TestFalse(TEXT("Dash stops representing Space while quick get-up is available"), DashAbility->SatisfiesDisplayContext(OwnedTags, DisplayContextTags));
	TestTrue(TEXT("Quick get-up represents Space while it is available"), QuickGetUpAbility->SatisfiesDisplayContext(OwnedTags, DisplayContextTags));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSAbilityDisplaySharedDefinitionTest, "RS.UI.AbilityDisplayContext.SharedDefinition", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSAbilityDisplaySharedDefinitionTest::RunTest(const FString& Parameters)
{
	UClass* Step01Class = LoadClass<URSBaseGameplayAbility>(nullptr, TEXT("/Game/AbilitySystem/Abilities/Player/GA_BasicAttack_Step01.GA_BasicAttack_Step01_C"));
	UClass* Step02Class = LoadClass<URSBaseGameplayAbility>(nullptr, TEXT("/Game/AbilitySystem/Abilities/Player/GA_BasicAttack_Step02.GA_BasicAttack_Step02_C"));
	UClass* DashClass = LoadClass<URSBaseGameplayAbility>(nullptr, TEXT("/Game/AbilitySystem/Abilities/Player/GA_Dash.GA_Dash_C"));
	TestNotNull(TEXT("Basic attack step 01 class"), Step01Class);
	TestNotNull(TEXT("Basic attack step 02 class"), Step02Class);
	TestNotNull(TEXT("Dash class"), DashClass);
	if (!Step01Class || !Step02Class || !DashClass)
	{
		return false;
	}

	const URSBaseGameplayAbility* Step01Ability = Step01Class->GetDefaultObject<URSBaseGameplayAbility>();
	const URSBaseGameplayAbility* Step02Ability = Step02Class->GetDefaultObject<URSBaseGameplayAbility>();
	const URSBaseGameplayAbility* DashAbility = DashClass->GetDefaultObject<URSBaseGameplayAbility>();
	const URSAbilityDefinition* BasicAttackDefinition = Step01Ability->GetAbilityDefinition();
	TestNotNull(TEXT("Basic attack definition"), BasicAttackDefinition);
	TestEqual(TEXT("Both combo steps share one definition"), Step02Ability->GetAbilityDefinition(), BasicAttackDefinition);
	TestNotEqual(TEXT("Dash uses a different definition"), DashAbility->GetAbilityDefinition(), BasicAttackDefinition);

	UWorld* TestWorld = CreateAbilityDisplayTestWorld();
	TestNotNull(TEXT("Ability display test world"), TestWorld);
	if (!TestWorld)
	{
		return false;
	}

	AActor* Owner = TestWorld->SpawnActor<AActor>();
	URSAbilitySystemComponent* AbilitySystemComponent = NewObject<URSAbilitySystemComponent>(Owner);
	AbilitySystemComponent->RegisterComponent();
	AbilitySystemComponent->InitAbilityActorInfo(Owner, Owner);

	GiveDisplayAbility(*AbilitySystemComponent, Step01Class, RSGameplayTags::InputTag_Ability_BasicAttack);
	GiveDisplayAbility(*AbilitySystemComponent, Step02Class, RSGameplayTags::InputTag_Ability_BasicAttack);

	const FRSAbilityDisplayResolveResult SharedResult = AbilitySystemComponent->ResolveDisplayAbilityForInputTag(RSGameplayTags::InputTag_Ability_BasicAttack);
	TestEqual(TEXT("Abilities with one definition resolve as one presentation"), SharedResult.Status, ERSAbilityDisplayResolveStatus::Resolved);
	TestEqual(TEXT("The shared definition represents the slot"), SharedResult.AbilityDefinition, BasicAttackDefinition);
	TestFalse(TEXT("A presentation group does not choose one cooldown handle"), SharedResult.AbilityHandle.IsValid());

	GiveDisplayAbility(*AbilitySystemComponent, DashClass, RSGameplayTags::InputTag_Ability_BasicAttack);
	const FRSAbilityDisplayResolveResult AmbiguousResult = AbilitySystemComponent->ResolveDisplayAbilityForInputTag(RSGameplayTags::InputTag_Ability_BasicAttack);
	TestEqual(TEXT("Different definitions remain ambiguous"), AmbiguousResult.Status, ERSAbilityDisplayResolveStatus::Ambiguous);
	TestNull(TEXT("An ambiguous result exposes no definition"), AmbiguousResult.AbilityDefinition);
	TestFalse(TEXT("An ambiguous result exposes no cooldown handle"), AmbiguousResult.AbilityHandle.IsValid());

	DestroyAbilityDisplayTestWorld(TestWorld);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSAbilityDisplayInputKeysTest, "RS.UI.AbilityDisplayContext.InputKeys", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSAbilityDisplayInputKeysTest::RunTest(const FString& Parameters)
{
	const FRSAbilityInputKeyDisplay BasicAttackDisplay = URSPlayerAbilityViewModel::BuildInputKeyDisplay({ EKeys::Q, EKeys::LeftMouseButton });
	TestEqual(TEXT("Mouse input is the primary basic attack binding"), BasicAttackDisplay.PrimaryInputKeyText.ToString(), FString(TEXT("LMB")));
	TestEqual(TEXT("Keyboard input is the secondary basic attack binding"), BasicAttackDisplay.SecondaryInputKeyText.ToString(), FString(TEXT("Q")));
	TestEqual(TEXT("Both basic attack bindings fit the existing key label"), BasicAttackDisplay.GetCombinedInputKeyText().ToString(), FString(TEXT("LMB / Q")));

	const FRSAbilityInputKeyDisplay DuplicateDisplay = URSPlayerAbilityViewModel::BuildInputKeyDisplay({ EKeys::LeftMouseButton, EKeys::LeftMouseButton, EKeys::Q });
	TestEqual(TEXT("Duplicate mappings keep one primary binding"), DuplicateDisplay.PrimaryInputKeyText.ToString(), FString(TEXT("LMB")));
	TestEqual(TEXT("Duplicate mappings do not displace the secondary binding"), DuplicateDisplay.SecondaryInputKeyText.ToString(), FString(TEXT("Q")));

	const FRSAbilityInputKeyDisplay SingleDisplay = URSPlayerAbilityViewModel::BuildInputKeyDisplay({ EKeys::SpaceBar });
	TestEqual(TEXT("A single keyboard mapping remains primary"), SingleDisplay.PrimaryInputKeyText.ToString(), FString(TEXT("Space bar")));
	TestTrue(TEXT("A single mapping has no secondary binding"), SingleDisplay.SecondaryInputKeyText.IsEmpty());
	TestEqual(TEXT("A single mapping does not add a separator"), SingleDisplay.GetCombinedInputKeyText().ToString(), FString(TEXT("Space bar")));

	const FRSAbilityInputKeyDisplay GamepadDisplay = URSPlayerAbilityViewModel::BuildInputKeyDisplay({ EKeys::Gamepad_FaceButton_Bottom });
	TestFalse(TEXT("Gamepad mapping is used when keyboard and mouse are absent"), GamepadDisplay.PrimaryInputKeyText.IsEmpty());
	TestTrue(TEXT("One gamepad mapping has no secondary binding"), GamepadDisplay.SecondaryInputKeyText.IsEmpty());

	return true;
}

#endif
