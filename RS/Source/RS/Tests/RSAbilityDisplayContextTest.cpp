#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Abilities/RSGameplayAbility_Dash.h"
#include "Abilities/RSGameplayAbility_GetUpQuick.h"
#include "RSAbilitySystemComponent.h"
#include "RSGameplayTags.h"

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

#endif
