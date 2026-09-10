// Fill out your copyright notice in the Description page of Project Settings.


#include "RSBaseGameplayAbility_MainGimmick.h"

#include "GameFramework/Pawn.h"
#include "RSBossCharacter.h"
#include "RSBossEncounter.h"
#include "RSBossPhaseComponent.h"

URSBaseGameplayAbility_MainGimmick::URSBaseGameplayAbility_MainGimmick()
{
	// 기믹은 전환 시퀀스가 종료를 기다리므로 인스턴스가 실행별 판정 상태를 가져야 합니다
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void URSBaseGameplayAbility_MainGimmick::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// 같은 인스턴스를 재사용하므로 이전 실행의 판정이 남지 않게 시작할 때 초기화합니다
	bGimmickBroken = false;

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void URSBaseGameplayAbility_MainGimmick::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 취소는 판정에 도달하지 못한 경우이므로 실패로 보고하지 않고 판정 없음으로 남깁니다
	if (!bWasCancelled)
	{
		AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
		if (URSBossPhaseComponent* PhaseComponent = URSBossPhaseComponent::FindPhaseComponent(Cast<APawn>(AvatarActor)))
		{
			// Super가 OnAbilityEnded를 알리면 Behavior Tree가 다음 노드로 넘어가므로 그 전에 보고합니다
			PhaseComponent->ReportMainGimmickOutcome(bGimmickBroken ? ERSBossMainGimmickOutcome::Broken : ERSBossMainGimmickOutcome::NotBroken);
		}

		if (!bGimmickBroken)
		{
			ApplyFailureDamageToParticipants();
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void URSBaseGameplayAbility_MainGimmick::MarkGimmickBroken()
{
	bGimmickBroken = true;
}

void URSBaseGameplayAbility_MainGimmick::ApplyFailureDamageToParticipants()
{
	if (!FailureDamageEffectClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s에 FailureDamageEffectClass가 없어 파훼 실패 피해를 적용하지 못했습니다"), *GetName());

		return;
	}

	const ARSBossCharacter* BossCharacter = Cast<ARSBossCharacter>(GetAvatarActorFromActorInfo());
	const ARSBossEncounter* BossEncounter = BossCharacter ? BossCharacter->GetBossEncounter() : nullptr;
	if (!BossEncounter)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s가 Encounter를 찾지 못해 파훼 실패 피해를 적용하지 못했습니다"), *GetName());

		return;
	}

	// 파훼 실패는 전멸기이므로 대상 하나가 아니라 살아 있는 참가자 전원에게 적용합니다
	TArray<APawn*> ParticipantPawns;
	BossEncounter->GetActiveParticipantPawns(ParticipantPawns);

	for (APawn* ParticipantPawn : ParticipantPawns)
	{
		ApplyDamageToTarget(ParticipantPawn, FailureDamageEffectClass, FailureDamage);
	}
}
