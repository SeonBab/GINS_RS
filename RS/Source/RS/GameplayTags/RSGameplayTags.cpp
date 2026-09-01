// Fill out your copyright notice in the Description page of Project Settings.


#include "RSGameplayTags.h"

namespace RSGameplayTags
{
	// Input
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_MoveTo, "InputTag.MoveTo", "Move to the clicked world location");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Ability_Sprint, "InputTag.Ability.Sprint", "");

	// Ability
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Movement_Sprint, "Ability.Movement.Sprint", "");



	// State
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Movement_Sprinting, "State.Movement.Sprinting", "");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dead, "State.Dead", "Character is dead");


}
