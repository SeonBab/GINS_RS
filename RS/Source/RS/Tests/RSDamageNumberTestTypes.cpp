// Fill out your copyright notice in the Description page of Project Settings.


#include "RSDamageNumberTestTypes.h"

#include "RSDamageNumberViewModel.h"

void URSDamageNumberTestListener::ListenTo(URSDamageNumberViewModel* InViewModel)
{
	if (!InViewModel)
	{
		return;
	}

	InViewModel->OnDamageNumberRequested.AddUniqueDynamic(this, &ThisClass::HandleDamageNumberRequested);
	InViewModel->OnDamageNumberResetRequested.AddUniqueDynamic(this, &ThisClass::HandleDamageNumberResetRequested);
}

void URSDamageNumberTestListener::ClearRecords()
{
	RequestedTexts.Reset();
	RequestedAnchors.Reset();
	ResetCount = 0;
}

void URSDamageNumberTestListener::HandleDamageNumberRequested(FText DisplayDamageText, FVector WorldAnchor)
{
	RequestedTexts.Add(DisplayDamageText.ToString());
	RequestedAnchors.Add(WorldAnchor);
}

void URSDamageNumberTestListener::HandleDamageNumberResetRequested()
{
	++ResetCount;
}
