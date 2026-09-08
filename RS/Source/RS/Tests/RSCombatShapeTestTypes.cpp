// Fill out your copyright notice in the Description page of Project Settings.

#include "RSCombatShapeTestTypes.h"

#include "Components/BoxComponent.h"
#include "RSAbilitySystemComponent.h"

ARSCombatShapeTestActor::ARSCombatShapeTestActor()
{
	CollisionComp = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision"));
	SetRootComponent(CollisionComp);
	CollisionComp->SetBoxExtent(FVector(1.0f));
	CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionComp->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionComp->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Overlap);
	CollisionComp->SetCanEverAffectNavigation(false);

	AbilitySystemComp = CreateDefaultSubobject<URSAbilitySystemComponent>(TEXT("AbilitySystem"));
}

UAbilitySystemComponent* ARSCombatShapeTestActor::GetAbilitySystemComponent() const
{
	return AbilitySystemComp;
}
