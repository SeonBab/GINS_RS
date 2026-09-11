// Fill out your copyright notice in the Description page of Project Settings.


#include "RSBaseGameplayAbility_BossPattern.h"

#include "GameFramework/Pawn.h"
#include "RSBossPhaseComponent.h"

void URSBaseGameplayAbility_BossPattern::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	bAnyTargetHit = false;

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void URSBaseGameplayAbility_BossPattern::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 취소는 판정에 도달하지 못한 경우이므로 보고하지 않고 판정 없음으로 남깁니다
	// 파훼 판정이 없는 패턴도 같은 이유로 보고하지 않습니다
	if (!bWasCancelled && HasGimmickBreakCondition())
	{
		AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
		if (URSBossPhaseComponent* PhaseComponent = URSBossPhaseComponent::FindPhaseComponent(Cast<APawn>(AvatarActor)))
		{
			// Super가 OnAbilityEnded를 알리면 Behavior Tree가 다음 노드로 넘어가므로 그 전에 보고합니다
			// 이번 실행이 메인 기믹이었는지는 페이즈 상태를 아는 컴포넌트가 판정합니다
			PhaseComponent->ReportMainGimmickOutcome(this, IsGimmickBroken() ? ERSBossMainGimmickOutcome::Broken : ERSBossMainGimmickOutcome::NotBroken);
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void URSBaseGameplayAbility_BossPattern::ApplyDamageToTarget(AActor* TargetActor, TSubclassOf<UGameplayEffect> DamageEffectClass, float DamageAmount)
{
	// 피해가 실제로 들어갔는지가 아니라 판정에 걸렸는지를 셉니다
	// 대상이 없는 호출은 판정에 걸리지 않은 것이므로 세지 않습니다
	if (TargetActor)
	{
		bAnyTargetHit = true;
	}

	Super::ApplyDamageToTarget(TargetActor, DamageEffectClass, DamageAmount);
}
