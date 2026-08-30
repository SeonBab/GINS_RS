// Fill out your copyright notice in the Description page of Project Settings.


#include "RSAttributeSet.h"

#include "RSAbilitySystemComponent.h"

URSAbilitySystemComponent* URSAttributeSet::GetRSAbilitySystemComponent() const
{
	return Cast<URSAbilitySystemComponent>(GetOwningAbilitySystemComponent());
}
