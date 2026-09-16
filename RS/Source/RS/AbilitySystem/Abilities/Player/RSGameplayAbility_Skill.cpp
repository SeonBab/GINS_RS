// Fill out your copyright notice in the Description page of Project Settings.

#include "RSGameplayAbility_Skill.h"

#include "GameFramework/Character.h"
#include "RSPlayerController.h"

URSGameplayAbility_Skill::URSGameplayAbility_Skill()
{
	ActivationPolicy = ERSAbilityActivationPolicy::OnInputTriggered;
}

void URSGameplayAbility_Skill::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid() || !SkillMontage || !FMath::IsFinite(SkillMontagePlayRate) || SkillMontagePlayRate <= 0.0f)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	// 커서 방향 회전과 이동 중단은 플레이어가 조종하는 아바타에서만 의미가 있습니다
	ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
	ARSPlayerController* PlayerController = Cast<ARSPlayerController>(ActorInfo->PlayerController.Get());
	if (!Character || !PlayerController)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	const FVector SkillDirection = GetCursorDirectionOrForward(ActorInfo, *Character);
	if (SkillDirection.IsNearlyZero())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	// 방향까지 확인한 뒤 Commit하여 실패한 스킬이 쿨다운을 소비하지 않게 합니다
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	// 스킬 시작 이후 Navigation 경로 추종이 회전과 충돌하지 않게 현재 이동 요청을 중단합니다
	PlayerController->StopMovement();

	const FRotator SkillRotation = SkillDirection.Rotation();
	Character->SetActorRotation(FRotator(0.0, SkillRotation.Yaw, 0.0));

	StartAttackMontage();
}

UAnimMontage* URSGameplayAbility_Skill::GetAttackMontage() const
{
	return SkillMontage;
}

float URSGameplayAbility_Skill::GetAttackMontagePlayRate() const
{
	return SkillMontagePlayRate;
}
