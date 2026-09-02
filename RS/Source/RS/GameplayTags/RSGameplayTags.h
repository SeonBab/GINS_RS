// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "NativeGameplayTags.h"

namespace RSGameplayTags
{
	// Ability
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Combat_BasicAttack);
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Movement_Dash);

	// Cooldown
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Ability_Dash);
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Active);

	// Input
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Ability_BasicAttack);
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Ability_Dash);
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_MoveTo);

	// SetByCaller
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Combo_Duration);
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Cooldown_Duration);

	// State
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Action_Locked);
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Combo_BasicAttack_Ready);
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Combo_BasicAttack_Ready_Step02);
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Combo_BasicAttack_Ready_Step03);
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dead);
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Defense_Evading);
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Defense_Invulnerable);
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Defense_KnockbackImmune);
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Movement_Dashing);
}
