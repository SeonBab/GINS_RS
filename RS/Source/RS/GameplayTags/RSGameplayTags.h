// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "NativeGameplayTags.h"

namespace RSGameplayTags
{
	// Ability
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Combat_BasicAttack);
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_CrowdControl_Downed);
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_CrowdControl_HitReact);
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Movement_Dash);
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Recovery_GetUp_Normal);
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Recovery_GetUp_Quick);

	// Cooldown
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Ability_Dash);
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Ability_GetUp_Quick);
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Active);

	// GameplayEvent
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayEvent_Combat_HitCheck);
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayEvent_CrowdControl_HitReact);
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayEvent_CrowdControl_Knockdown);

	// Input
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Ability_BasicAttack);
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Ability_Dash);
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Ability_SkillSlot01);
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Ability_SkillSlot02);
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Ability_SkillSlot03);
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_MoveTo);

	// SetByCaller
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Combo_Duration);
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Cooldown_Duration);
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Damage);

	// State
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Action_Locked);
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Combo_BasicAttack_Ready);
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Combo_BasicAttack_Ready_Step02);
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Combo_BasicAttack_Ready_Step03);
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_CrowdControl_Downed);
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_CrowdControl_HitReact);
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_CrowdControl_Knockdown);
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_CrowdControl_GettingUp);
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dead);
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Immunity_Damage);
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Immunity_HitReact);
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Immunity_Knockback);
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Movement_Blocked);
	RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Movement_Dashing);
}
