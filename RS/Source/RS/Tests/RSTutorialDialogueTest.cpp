#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "RSTutorialDialogue.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSTutorialDialogueDefaultsTest, "RS.Interaction.TutorialDialogue.Defaults", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSTutorialDialogueDefaultsTest::RunTest(const FString& Parameters)
{
	const ARSTutorialDialogue* DialogueDefault = GetDefault<ARSTutorialDialogue>();
	TestNotNull(TEXT("Tutorial dialogue default object exists"), DialogueDefault);
	if (!DialogueDefault)
	{
		return false;
	}

	TestTrue(TEXT("Tutorial dialogue implements the interactable contract"), ARSTutorialDialogue::StaticClass()->ImplementsInterface(URSInteractable::StaticClass()));
	TestFalse(TEXT("Tutorial dialogue does not tick"), DialogueDefault->PrimaryActorTick.bCanEverTick);

	const UBoxComponent* InteractionBounds = DialogueDefault->FindComponentByClass<UBoxComponent>();
	TestNotNull(TEXT("Tutorial dialogue has interaction bounds"), InteractionBounds);
	if (InteractionBounds)
	{
		TestEqual(TEXT("Interaction bounds uses the dedicated profile"), InteractionBounds->GetCollisionProfileName(), FName(TEXT("RSInteractable")));
	}

	const UStaticMeshComponent* DisplayMesh = DialogueDefault->FindComponentByClass<UStaticMeshComponent>();
	TestNotNull(TEXT("Tutorial dialogue has a display mesh"), DisplayMesh);
	if (DisplayMesh)
	{
		TestNull(TEXT("Display mesh is intentionally unassigned"), DisplayMesh->GetStaticMesh());
		TestEqual(TEXT("Display mesh ignores the interactable channel"), DisplayMesh->GetCollisionResponseToChannel(ECC_GameTraceChannel3), ECR_Ignore);
	}

	const USceneComponent* PromptAnchor = DialogueDefault->GetPromptAnchor_Implementation();
	TestNotNull(TEXT("Tutorial dialogue has a prompt anchor"), PromptAnchor);
	if (PromptAnchor)
	{
		TestEqual(TEXT("Prompt anchor keeps the approved component name"), PromptAnchor->GetFName(), FName(TEXT("PromptAnchor")));
	}

	const FTextProperty* DialogueTextProperty = FindFProperty<FTextProperty>(ARSTutorialDialogue::StaticClass(), TEXT("DialogueText"));
	TestNotNull(TEXT("DialogueText property exists"), DialogueTextProperty);
	if (DialogueTextProperty)
	{
		TestTrue(TEXT("DialogueText is editable"), DialogueTextProperty->HasAnyPropertyFlags(CPF_Edit));
		TestFalse(TEXT("DialogueText is editable on placed instances"), DialogueTextProperty->HasAnyPropertyFlags(CPF_DisableEditOnInstance));
		TestTrue(TEXT("DialogueText supports multiline editing"), DialogueTextProperty->HasMetaData(TEXT("MultiLine")));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
