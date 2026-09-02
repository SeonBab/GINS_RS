// Fill out your copyright notice in the Description page of Project Settings.


#include "RSGameplayTags.h"

namespace RSGameplayTags
{
	// Ability
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Combat_BasicAttack, "Ability.Combat.BasicAttack", "플레이어 기본 공격 Ability를 식별합니다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Movement_Dash, "Ability.Movement.Dash", "대시 이동 Ability를 식별합니다");

	// Cooldown
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Ability_Dash, "Cooldown.Ability.Dash", "쿨다운이 끝날 때까지 대시 재활성화를 차단합니다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Active, "Cooldown.Active", "하나 이상의 Ability 쿨다운이 활성화된 상태를 나타냅니다");

	// Input
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Ability_BasicAttack, "InputTag.Ability.BasicAttack", "기본 공격 Ability 입력을 식별합니다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Ability_Dash, "InputTag.Ability.Dash", "대시 Ability 입력을 식별합니다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_MoveTo, "InputTag.MoveTo", "클릭한 월드 위치로 이동하는 입력을 식별합니다");

	// SetByCaller
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Combo_Duration, "SetByCaller.Combo.Duration", "콤보의 다음 공격 준비 상태가 유지될 시간입니다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Cooldown_Duration, "SetByCaller.Cooldown.Duration", "Ability마다 다른 쿨다운 시간을 공용 쿨다운 GameplayEffect에 전달합니다");

	// State
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Action_Locked, "State.Action.Locked", "Montage의 행동 잠금 구간처럼 다른 Ability의 활성화를 차단하는 상태입니다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Combo_BasicAttack_Ready, "State.Combo.BasicAttack.Ready", "기본 공격 콤보 준비 단계 태그의 상위 태그이며 직접 부여하지 않습니다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Combo_BasicAttack_Ready_Step02, "State.Combo.BasicAttack.Ready.Step02", "다음 기본 공격 입력으로 2타를 시작할 수 있는 상태입니다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Combo_BasicAttack_Ready_Step03, "State.Combo.BasicAttack.Ready.Step03", "다음 기본 공격 입력으로 3타를 시작할 수 있는 상태입니다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dead, "State.Dead", "캐릭터가 사망한 상태이며 Ability 활성화와 입력 처리를 차단합니다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Defense_Evading, "State.Defense.Evading", "대미지와 경직에는 면역이지만 넉백은 허용하는 회피 상태입니다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Defense_Invulnerable, "State.Defense.Invulnerable", "대미지와 모든 적대적 상태이상에 면역인 상태입니다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Defense_KnockbackImmune, "State.Defense.KnockbackImmune", "넉백에만 면역인 상태입니다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Movement_Dashing, "State.Movement.Dashing", "대시 이동이 실행되는 동안 적용합니다");
}
