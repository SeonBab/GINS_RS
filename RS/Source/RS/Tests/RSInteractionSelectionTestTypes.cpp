// Fill out your copyright notice in the Description page of Project Settings.

#if WITH_TESTS

#include "RSInteractionSelectionTestTypes.h"

#include "Components/SphereComponent.h"

ARSInteractionSelectionTestTarget::ARSInteractionSelectionTestTarget()
{
	PrimaryActorTick.bCanEverTick = false;

	Shape = CreateDefaultSubobject<USphereComponent>(TEXT("Shape"));
	SetRootComponent(Shape);

	Shape->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Shape->SetSphereRadius(50.0f);
}

ARSInteractionSelectionTestNonTarget::ARSInteractionSelectionTestNonTarget()
{
	PrimaryActorTick.bCanEverTick = false;

	Shape = CreateDefaultSubobject<USphereComponent>(TEXT("Shape"));
	SetRootComponent(Shape);

	Shape->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Shape->SetSphereRadius(50.0f);
}

#endif // WITH_TESTS
