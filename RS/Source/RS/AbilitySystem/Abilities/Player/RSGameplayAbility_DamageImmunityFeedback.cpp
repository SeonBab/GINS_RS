// Fill out your copyright notice in the Description page of Project Settings.

#include "RSGameplayAbility_DamageImmunityFeedback.h"

#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Combat/RSCombatFunctionLibrary.h"
#include "RSGameplayTags.h"

URSGameplayAbility_DamageImmunityFeedback::URSGameplayAbility_DamageImmunityFeedback()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// 무적으로 막았다는 통지는 피해를 처리한 쪽에서만 오므로 입력이나 상태가 아니라 이벤트로만 활성화합니다
	FAbilityTriggerData TriggerData;
	TriggerData.TriggerTag = RSGameplayTags::GameplayEvent_Presentation_DamageImmunity;
	TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(TriggerData);
}

void URSGameplayAbility_DamageImmunityFeedback::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// 광역과 다단 히트는 무적 구간 안에서 여러 번 막히므로 같은 연출이 겹치지 않도록 공용 쿨다운으로 제한합니다
	// 쿨다운을 설정하지 않은 하위 클래스는 막은 타격마다 재생합니다
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	const AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	if (AvatarActor)
	{
		// 막은 주체가 자기 자신이므로 공격자의 위치가 아니라 Avatar를 기준으로 재생합니다
		const FTransform AvatarTransform = AvatarActor->GetActorTransform();
		USkeletalMeshComponent* MeshComponent = ActorInfo->SkeletalMeshComponent.Get();
		URSCombatFunctionLibrary::SpawnNiagaraFromDefinition(this, MeshComponent, ImmunityNiagara, AvatarTransform, true);

		if (ImmunitySound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, ImmunitySound, AvatarTransform.GetLocation());
		}
	}

	// 연출은 한 프레임 안에 시작하고 끝나므로 활성 상태를 남기지 않습니다
	// 남기면 다음 타격의 연출 요청이 이미 활성 상태에 막혀 조용히 사라집니다
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
