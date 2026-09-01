// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "NativeGameplayTags.h"

namespace RSGameplayTags
{
	// Input
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_MoveTo);
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Ability_Sprint);

	// Ability
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Movement_Sprint);

	// State
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Movement_Sprinting);
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dead);

}
