// Fill out your copyright notice in the Description page of Project Settings.


#include "RSBaseCharacter.h"

// Sets default values
ARSBaseCharacter::ARSBaseCharacter()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}
// Called when the game starts or when spawned
void ARSBaseCharacter::BeginPlay()
{
	Super::BeginPlay();

}

// Called every frame
void ARSBaseCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}
