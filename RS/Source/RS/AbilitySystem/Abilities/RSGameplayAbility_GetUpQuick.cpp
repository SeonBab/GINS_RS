// Fill out your copyright notice in the Description page of Project Settings.

#include "RSGameplayAbility_GetUpQuick.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "RSGameplayTags.h"
#include "Tasks/RSAbilityTask_CurveMovement.h"

URSGameplayAbility_GetUpQuick::URSGameplayAbility_GetUpQuick()
{
	ActivationPolicy = ERSAbilityActivationPolicy::OnInputTriggered;

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(RSGameplayTags::Ability_Recovery_GetUp_Quick);
	SetAssetTags(AssetTags);

	CooldownTags.AddTag(RSGameplayTags::Cooldown_Ability_GetUp_Quick);
	CooldownDuration = 1.0f;

	// 초반에 빠르게 굴러 나가고 후반에 잦아드는 개발용 곡선입니다
	FRichCurve* ProgressCurve = DistanceProgressCurve.GetRichCurve();
	ProgressCurve->AddKey(0.0f, 0.0f);
	ProgressCurve->AddKey(0.25f, 0.5f);
	ProgressCurve->AddKey(0.6f, 0.85f);
	ProgressCurve->AddKey(1.0f, 1.0f);
}

void URSGameplayAbility_GetUpQuick::StartGetUpMovement(ACharacter& Character)
{
	UCharacterMovementComponent* MovementComponent = Character.GetCharacterMovement();
	const FRichCurve* ProgressCurve = DistanceProgressCurve.GetRichCurveConst();
	if (!MovementComponent || !ProgressCurve || ProgressCurve->GetNumKeys() < 2 || RollDistance <= 0.0f || RollDuration <= 0.0f)
	{
		return;
	}

	const FVector RollDirection = GetCursorDirectionOrForward(CurrentActorInfo, Character);
	if (RollDirection.IsNearlyZero())
	{
		return;
	}

	const FRotator RollRotation = RollDirection.Rotation();
	Character.SetActorRotation(FRotator(0.0, RollRotation.Yaw, 0.0));

	// Task의 정리는 UGameplayAbility::EndAbility가 담당하므로 어빌리티가 핸들을 보관하지 않습니다
	URSAbilityTask_CurveMovement* RollTask = URSAbilityTask_CurveMovement::CreateCurveMovement(this, MovementComponent, RollDirection, RollDistance, RollDuration, *ProgressCurve);
	RollTask->ReadyForActivation();
}
