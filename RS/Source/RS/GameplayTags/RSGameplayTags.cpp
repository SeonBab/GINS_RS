// Fill out your copyright notice in the Description page of Project Settings.


#include "RSGameplayTags.h"

namespace RSGameplayTags
{
	// Ability
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Combat_BasicAttack, "Ability.Combat.BasicAttack", "플레이어 기본 공격 Ability를 식별합니다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Combat_ConcentricRings, "Ability.Combat.ConcentricRings", "보스 동심원 링 순서 암기 패턴 Ability를 식별합니다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_CrowdControl_Downed, "Ability.CrowdControl.Downed", "누워 기상을 기다리는 Ability를 식별하며 기상과 넉다운이 이 Ability를 취소합니다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_CrowdControl_HitReact, "Ability.CrowdControl.HitReact", "피격 경직 Ability를 식별하며 넉다운이 진행 중인 경직을 취소합니다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Movement_Dash, "Ability.Movement.Dash", "대시 이동 Ability를 식별합니다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Recovery_GetUp_Normal, "Ability.Recovery.GetUp.Normal", "자동 조건으로 실행되는 일반 기상 Ability를 식별합니다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Recovery_GetUp_Quick, "Ability.Recovery.GetUp.Quick", "플레이어 입력으로 실행되는 빠른 기상 Ability를 식별합니다");

	// Cooldown
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Ability_Dash, "Cooldown.Ability.Dash", "쿨다운이 끝날 때까지 대시 재활성화를 차단합니다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Ability_GetUp_Quick, "Cooldown.Ability.GetUp.Quick", "쿨다운이 끝날 때까지 빠른 기상 재활성화를 차단합니다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Active, "Cooldown.Active", "하나 이상의 Ability 쿨다운이 활성화된 상태를 나타냅니다");

	// GameplayEvent
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayEvent_Combat_HitCheck, "GameplayEvent.Combat.HitCheck", "Montage의 타격 시점에 공격 판정을 실행하도록 Ability에 알립니다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayEvent_CrowdControl_HitReact, "GameplayEvent.CrowdControl.HitReact", "공격이 대상에게 피격 경직을 요청하며 대상의 경직 Ability를 활성화합니다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayEvent_CrowdControl_Knockdown, "GameplayEvent.CrowdControl.Knockdown", "공격이 대상에게 넉다운을 요청하며 대상의 넉다운 Ability를 활성화합니다");

	// Input
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Ability_BasicAttack, "InputTag.Ability.BasicAttack", "기본 공격 Ability 입력을 식별합니다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Ability_Dash, "InputTag.Ability.Dash", "대시 Ability 입력을 식별합니다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Ability_SkillSlot01, "InputTag.Ability.SkillSlot01", "1번 스킬 슬롯에 배치된 Ability 입력을 식별합니다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Ability_SkillSlot02, "InputTag.Ability.SkillSlot02", "2번 스킬 슬롯에 배치된 Ability 입력을 식별합니다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Ability_SkillSlot03, "InputTag.Ability.SkillSlot03", "3번 스킬 슬롯에 배치된 Ability 입력을 식별합니다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_MoveTo, "InputTag.MoveTo", "클릭한 월드 위치로 이동하는 입력을 식별합니다");

	// SetByCaller
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Combo_Duration, "SetByCaller.Combo.Duration", "콤보의 다음 공격 준비 상태가 유지될 시간입니다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Cooldown_Duration, "SetByCaller.Cooldown.Duration", "Ability마다 다른 쿨다운 시간을 공용 쿨다운 GameplayEffect에 전달합니다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Damage, "SetByCaller.Damage", "타격마다 다른 피해량을 공용 대미지 GameplayEffect에 전달합니다");

	// State
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Action_Locked, "State.Action.Locked", "Montage의 행동 잠금 구간처럼 다른 Ability의 활성화를 차단하는 상태입니다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Combo_BasicAttack_Ready, "State.Combo.BasicAttack.Ready", "기본 공격 콤보 준비 단계 태그의 상위 태그이며 직접 부여하지 않습니다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Combo_BasicAttack_Ready_Step02, "State.Combo.BasicAttack.Ready.Step02", "다음 기본 공격 입력으로 2타를 시작할 수 있는 상태입니다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Combo_BasicAttack_Ready_Step03, "State.Combo.BasicAttack.Ready.Step03", "다음 기본 공격 입력으로 3타를 시작할 수 있는 상태입니다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_CrowdControl_Downed, "State.CrowdControl.Downed", "넉다운이 끝나고 바닥에 누워 기상을 기다리는 상태입니다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_CrowdControl_HitReact, "State.CrowdControl.HitReact", "피격 경직 상태입니다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_CrowdControl_Knockdown, "State.CrowdControl.Knockdown", "다운 공격을 받고 바닥으로 넘어지는 중인 상태입니다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_CrowdControl_GettingUp, "State.CrowdControl.GettingUp", "누운 상태에서 일반 또는 빠른 기상으로 일어나는 중인 상태입니다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dead, "State.Dead", "캐릭터가 사망한 상태이며 Ability 활성화와 입력 처리를 차단합니다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Immunity_Damage, "State.Immunity.Damage", "현재 대상에게 대미지를 적용할 수 없는 상태입니다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Immunity_HitReact, "State.Immunity.HitReact", "현재 대상에게 피격 경직을 적용할 수 없는 상태입니다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Immunity_Knockback, "State.Immunity.Knockback", "현재 대상에게 넉백을 적용할 수 없는 상태입니다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Movement_Blocked, "State.Movement.Blocked", "Navigation 이동 요청과 진행 중인 경로 추종을 차단합니다");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Movement_Dashing, "State.Movement.Dashing", "대시 이동이 실행되는 동안 적용합니다");
}
