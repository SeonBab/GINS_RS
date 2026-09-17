// Fill out your copyright notice in the Description page of Project Settings.

#if WITH_TESTS

#include "RSHealthInitializationTestTypes.h"

#include "RSHealthComponent.h"

void URSHealthInitializationTestListener::ListenTo(URSHealthComponent* InHealthComponent)
{
	if (!InHealthComponent)
	{
		return;
	}

	InHealthComponent->OnHealthChanged.AddUniqueDynamic(this, &ThisClass::HandleHealthChanged);
	InHealthComponent->OnMaxHealthChanged.AddUniqueDynamic(this, &ThisClass::HandleMaxHealthChanged);
}

void URSHealthInitializationTestListener::HandleHealthChanged(URSHealthComponent* InHealthComponent, float OldValue, float NewValue)
{
	HealthValues.Add(NewValue);
}

void URSHealthInitializationTestListener::HandleMaxHealthChanged(URSHealthComponent* InHealthComponent, float OldValue, float NewValue)
{
	MaxHealthValues.Add(NewValue);
}

#endif // WITH_TESTS
