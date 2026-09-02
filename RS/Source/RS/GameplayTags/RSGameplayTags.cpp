// Fill out your copyright notice in the Description page of Project Settings.


#include "RSGameplayTags.h"

namespace RSGameplayTags
{
	// Input
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_MoveTo, "InputTag.MoveTo", "Move to the clicked world location");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Ability_BasicAttack, "InputTag.Ability.BasicAttack", "");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Ability_Dash, "InputTag.Ability.Dash", "대시 Ability 입력을 식별합니다");

	// Ability
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Combat_BasicAttack, "Ability.Combat.BasicAttack", "플레이어 기본 공격 Ability를 식별합니다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Movement_Dash, "Ability.Movement.Dash", "대시 이동 Ability를 식별합니다");

	// State
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Action_Locked, "State.Action.Locked", "");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Defense_Evading, "State.Defense.Evading", "대미지와 경직에는 면역이지만 넉백은 허용하는 회피 상태입니다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Defense_KnockbackImmune, "State.Defense.KnockbackImmune", "넉백에만 면역인 상태입니다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Defense_Invulnerable, "State.Defense.Invulnerable", "대미지와 모든 적대적 상태이상에 면역인 상태입니다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dead, "State.Dead", "Character is dead");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Movement_Dashing, "State.Movement.Dashing", "대시 이동이 실행되는 동안 적용합니다");

	// Cooldown
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Active, "Cooldown.Active", "하나 이상의 Ability 쿨다운이 활성화된 상태를 나타냅니다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Ability_Dash, "Cooldown.Ability.Dash", "쿨다운이 끝날 때까지 대시 재활성화를 차단합니다");

	// SetByCaller
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Cooldown_Duration, "SetByCaller.Cooldown.Duration", "");
}
