// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "NativeGameplayTags.h"

namespace RSGameplayTags
{
	// Input
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_MoveTo);
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Ability_BasicAttack);
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Ability_Dash);

	// Ability
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Combat_BasicAttack);
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Movement_Dash);

	// State
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Action_Locked);
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Combat_Invulnerable);
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dead);
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Movement_Dashing);

	// Cooldown
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Active);
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Ability_Dash);

	// SetByCaller
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Cooldown_Duration);
}
