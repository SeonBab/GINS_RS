// Fill out your copyright notice in the Description page of Project Settings.

#include "RSGameplayAbility_Dash.h"

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Curves/CurveFloat.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "RSGameplayTags.h"
#include "RSPlayerController.h"

namespace
{
	const FName DashRootMotionSourceName(TEXT("Dash"));
	constexpr uint16 DashRootMotionPriority = 1000;
}

URSGameplayAbility_Dash::URSGameplayAbility_Dash()
{
	ActivationPolicy = ERSAbilityActivationPolicy::OnInputTriggered;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(RSGameplayTags::Ability_Movement_Dash);
	SetAssetTags(AssetTags);

	// Montage의 행동 잠금 구간에서는 활성화를 막고 잠금이 끝난 뒤 새 대시가 진행 중인 기본 공격을 교체하게 합니다
	ActivationBlockedTags.AddTag(RSGameplayTags::State_Action_Locked);
	CancelAbilitiesWithTag.AddTag(RSGameplayTags::Ability_Combat_BasicAttack);

	ActivationOwnedTags.AddTag(RSGameplayTags::State_Movement_Dashing);
	CooldownTags.AddTag(RSGameplayTags::Cooldown_Ability_Dash);
	CooldownDuration = 1.0f;

	// 외부 애셋이 없어도 초반 이동이 빠르고 후반이 감속하는 개발용 곡선을 바로 검증할 수 있게 합니다
	FRichCurve* ProgressCurve = DistanceProgressCurve.GetRichCurve();
	ProgressCurve->AddKey(0.0f, 0.0f);
	ProgressCurve->AddKey(0.15f, 0.45f);
	ProgressCurve->AddKey(0.4f, 0.78f);
	ProgressCurve->AddKey(0.75f, 0.96f);
	ProgressCurve->AddKey(1.0f, 1.0f);
}

void URSGameplayAbility_Dash::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
	if (!Character)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	UCharacterMovementComponent* MovementComponent = Character->GetCharacterMovement();
	const FRichCurve* ProgressCurve = DistanceProgressCurve.GetRichCurveConst();
	if (!MovementComponent || !ProgressCurve || ProgressCurve->GetNumKeys() < 2 || DashDistance <= 0.0f || DashDuration <= 0.0f)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	const FVector DashDirection = GetDashDirection(Character, ActorInfo);
	if (DashDirection.IsNearlyZero())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	// 모든 실행 조건을 확인한 뒤 Commit하여 실패한 대시가 쿨다운을 소비하지 않게 합니다
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	// Navigation 이동이 Root Motion Source와 동시에 캐릭터를 제어하지 않게 현재 경로를 중지합니다
	if (AController* Controller = Character->GetController())
	{
		Controller->StopMovement();
	}

	const FVector StartLocation = Character->GetActorLocation();
	const FVector TargetLocation = StartLocation + DashDirection * DashDistance;
	ActiveDistanceProgressCurve = NewObject<UCurveFloat>(this);
	ActiveDistanceProgressCurve->FloatCurve = *ProgressCurve;

	TSharedPtr<FRootMotionSource_MoveToDynamicForce> DashRootMotionSource = MakeShared<FRootMotionSource_MoveToDynamicForce>();
	DashRootMotionSource->InstanceName = DashRootMotionSourceName;
	DashRootMotionSource->Priority = DashRootMotionPriority;
	DashRootMotionSource->AccumulateMode = ERootMotionAccumulateMode::Override;
	DashRootMotionSource->Duration = DashDuration;
	DashRootMotionSource->StartLocation = StartLocation;
	DashRootMotionSource->InitialTargetLocation = TargetLocation;
	DashRootMotionSource->TargetLocation = TargetLocation;
	DashRootMotionSource->TimeMappingCurve = ActiveDistanceProgressCurve;
	DashRootMotionSource->bRestrictSpeedToExpected = true;
	DashRootMotionSource->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::SetVelocity;
	DashRootMotionSource->FinishVelocityParams.SetVelocity = FVector::ZeroVector;

	CachedMovementComponent = MovementComponent;
	ActiveRootMotionSourceID = MovementComponent->ApplyRootMotionSource(DashRootMotionSource);

	if (ActiveRootMotionSourceID == static_cast<uint16>(ERootMotionSourceID::Invalid))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	UAbilityTask_WaitDelay* DashDurationTask = UAbilityTask_WaitDelay::WaitDelay(this, DashDuration);
	DashDurationTask->OnFinish.AddDynamic(this, &ThisClass::HandleDashFinished);
	DashDurationTask->ReadyForActivation();
}

void URSGameplayAbility_Dash::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (ActiveRootMotionSourceID != static_cast<uint16>(ERootMotionSourceID::Invalid))
	{
		if (UCharacterMovementComponent* MovementComponent = CachedMovementComponent.Get())
		{
			// 다른 이동 효과를 건드리지 않고 현재 대시가 적용한 Source만 제거합니다
			MovementComponent->RemoveRootMotionSourceByID(ActiveRootMotionSourceID);
		}
	}

	ActiveRootMotionSourceID = static_cast<uint16>(ERootMotionSourceID::Invalid);
	CachedMovementComponent.Reset();
	ActiveDistanceProgressCurve = nullptr;

	// Commit에서 적용한 쿨다운은 대시가 취소되어도 원래 만료 시점까지 유지합니다
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

FVector URSGameplayAbility_Dash::GetDashDirection(const ACharacter* Character, const FGameplayAbilityActorInfo* ActorInfo) const
{
	if (!Character)
	{
		return FVector::ZeroVector;
	}

	if (ActorInfo)
	{
		const ARSPlayerController* PlayerController = Cast<ARSPlayerController>(ActorInfo->PlayerController.Get());
		if (PlayerController)
		{
			FVector CursorWorldLocation;
			if (PlayerController->GetCursorWorldLocation(CursorWorldLocation))
			{
				FVector CursorDirection = CursorWorldLocation - Character->GetActorLocation();
				CursorDirection.Z = 0.0f;

				if (CursorDirection.Normalize())
				{
					return CursorDirection;
				}
			}
		}
	}

	// 커서 Hit을 얻지 못하거나 캐릭터와 같은 위치를 가리키면 현재 전방을 안전한 대체 방향으로 사용합니다
	FVector ForwardDirection = Character->GetActorForwardVector();
	ForwardDirection.Z = 0.0f;
	ForwardDirection.Normalize();

	return ForwardDirection;
}

void URSGameplayAbility_Dash::HandleDashFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
