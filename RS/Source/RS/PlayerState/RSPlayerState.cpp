// Fill out your copyright notice in the Description page of Project Settings.


#include "RSPlayerState.h"

#include "RSAbilitySystemComponent.h"
#include "RSHealthSet.h"

ARSPlayerState::ARSPlayerState()
{
	AbilitySystemComp = CreateDefaultSubobject<URSAbilitySystemComponent>(TEXT("RSAbilitySystemComponent"));
	HealthSet = CreateDefaultSubobject<URSHealthSet>(TEXT("RSHealthSet"));
}

UAbilitySystemComponent* ARSPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComp;
}

URSAbilitySystemComponent* ARSPlayerState::GetRSAbilitySystemComponent() const
{
	return AbilitySystemComp;
}

const URSHealthSet* ARSPlayerState::GetHealthSet() const
{
	return HealthSet;
}
