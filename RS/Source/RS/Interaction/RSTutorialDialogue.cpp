// Fill out your copyright notice in the Description page of Project Settings.

#include "RSTutorialDialogue.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "RSPlayerController.h"

#define LOCTEXT_NAMESPACE "RSTutorialDialogue"

ARSTutorialDialogue::ARSTutorialDialogue()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	DisplayMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DisplayMesh"));
	DisplayMesh->SetupAttachment(SceneRoot);
	DisplayMesh->SetCollisionResponseToChannel(ECC_GameTraceChannel3, ECR_Ignore);

	InteractionBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBounds"));
	InteractionBounds->SetupAttachment(SceneRoot);
	InteractionBounds->SetCollisionProfileName(TEXT("RSInteractable"));

	PromptAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("PromptAnchor"));
	PromptAnchor->SetupAttachment(SceneRoot);

	PromptText = LOCTEXT("PromptText", "대화하기");
}

FText ARSTutorialDialogue::GetPromptText_Implementation() const
{
	return PromptText;
}

USceneComponent* ARSTutorialDialogue::GetPromptAnchor_Implementation() const
{
	return PromptAnchor;
}

void ARSTutorialDialogue::Interact_Implementation(AActor* InteractInstigator)
{
	const APawn* InteractPawn = Cast<APawn>(InteractInstigator);
	ARSPlayerController* PlayerController = InteractPawn ? Cast<ARSPlayerController>(InteractPawn->GetController()) : nullptr;
	if (PlayerController)
	{
		PlayerController->ToggleDialogue(this, DialogueText);
	}
}

#undef LOCTEXT_NAMESPACE
