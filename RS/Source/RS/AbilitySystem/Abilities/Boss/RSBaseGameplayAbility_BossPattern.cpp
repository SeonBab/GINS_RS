// Fill out your copyright notice in the Description page of Project Settings.


#include "RSBaseGameplayAbility_BossPattern.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "RSBossPhaseComponent.h"

void URSBaseGameplayAbility_BossPattern::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	bAnyTargetHit = false;

	// InstancedPerActor라 인스턴스가 재사용되므로 이전 실행의 Montage 게이트 상태를 먼저 되돌립니다
	bPatternTimelineFinished = false;
	PendingPatternMontageCount = 0;
	PatternMontageTasks.Reset();
	PlayedPatternMontages.Reset();

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void URSBaseGameplayAbility_BossPattern::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// Task를 끝내면 Montage가 중단되어 완료 Callback이 되돌아올 수 있으므로 게이트를 먼저 닫고 목록을 비웁니다
	TArray<TObjectPtr<UAbilityTask_PlayMontageAndWait>> TasksToEnd = MoveTemp(PatternMontageTasks);
	PatternMontageTasks.Reset();
	PendingPatternMontageCount = 0;
	bPatternTimelineFinished = false;

	for (UAbilityTask_PlayMontageAndWait* MontageTask : TasksToEnd)
	{
		if (MontageTask)
		{
			MontageTask->EndTask();
		}
	}

	// Montage가 중단되면 Notify State의 End가 오지 않아 상태 태그가 남으므로 여기서 회수합니다
	for (UAnimMontage* PlayedMontage : PlayedPatternMontages)
	{
		EndAnimationGameplayStatesForMontage(ActorInfo, PlayedMontage);
	}
	PlayedPatternMontages.Reset();

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

UAbilityTask_PlayMontageAndWait* URSBaseGameplayAbility_BossPattern::PlayPatternMontage(UAnimMontage* Montage, float PlayRate)
{
	if (!Montage)
	{
		return nullptr;
	}

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Montage, PlayRate);
	if (!MontageTask)
	{
		return nullptr;
	}

	// 정상 완료에서는 OnBlendOut과 OnCompleted가 모두 오므로 둘 다 묶으면 한 Montage가 개수를 두 번 내립니다
	// 여기서는 Montage가 끝까지 재생된 OnCompleted만 완료로 보고, 중단과 재생 실패는 더 기다릴 대상이 없으므로 같이 내립니다
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandlePatternMontageFinished);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandlePatternMontageFinished);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandlePatternMontageFinished);

	PatternMontageTasks.Add(MontageTask);
	PlayedPatternMontages.AddUnique(Montage);

	// 재생에 실패하면 엔진이 ReadyForActivation() 안에서 OnCancelled를 동기로 알리므로 개수를 먼저 올립니다
	++PendingPatternMontageCount;
	MontageTask->ReadyForActivation();

	return MontageTask;
}

void URSBaseGameplayAbility_BossPattern::FinishPatternWhenMontageEnds()
{
	bPatternTimelineFinished = true;

	TryFinishPattern();
}

void URSBaseGameplayAbility_BossPattern::PlayPatternPresentation(const TArray<FTransform>& NiagaraTransforms, const FVector& SoundLocation) const
{
	for (const FTransform& NiagaraTransform : NiagaraTransforms)
	{
		SpawnNiagaraFromDefinition(PatternPresentation.Niagara, NiagaraTransform, true);
	}

	// 광역 패턴은 Niagara가 여러 지점에서 나지만 소리까지 겹쳐 울리면 한 번의 공격으로 들리지 않습니다
	if (PatternPresentation.Sound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, PatternPresentation.Sound, SoundLocation);
	}

	URSCombatFunctionLibrary::PlayCameraShake(this, PatternPresentation.CameraShake);
}

void URSBaseGameplayAbility_BossPattern::PlayPatternPresentation(const FTransform& PresentationTransform) const
{
	PlayPatternPresentation(TArray<FTransform>({ PresentationTransform }), PresentationTransform.GetLocation());
}

UNiagaraComponent* URSBaseGameplayAbility_BossPattern::SpawnNiagaraFromDefinition(const FRSNiagaraSpawnDefinition& Definition, const FTransform& WorldTransform, bool bAutoDestroy) const
{
	if (!Definition.NiagaraSystem)
	{
		return nullptr;
	}

	if (Definition.SpawnMode == ERSNiagaraSpawnMode::WorldTransform)
	{
		return UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, Definition.NiagaraSystem, WorldTransform.GetLocation(), WorldTransform.Rotator(), WorldTransform.GetScale3D(), bAutoDestroy, true);
	}

	USkeletalMeshComponent* MeshComponent = CurrentActorInfo ? CurrentActorInfo->SkeletalMeshComponent.Get() : nullptr;
	if (!MeshComponent || !MeshComponent->DoesSocketExist(Definition.SocketName))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s skipped Niagara %s because socket %s is unavailable"), *GetName(), *GetNameSafe(Definition.NiagaraSystem), *Definition.SocketName.ToString());

		return nullptr;
	}

	if (Definition.SpawnMode == ERSNiagaraSpawnMode::SocketSnapshot)
	{
		const FTransform SocketTransform = MeshComponent->GetSocketTransform(Definition.SocketName);

		return UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, Definition.NiagaraSystem, SocketTransform.GetLocation(), SocketTransform.Rotator(), SocketTransform.GetScale3D(), bAutoDestroy, true);
	}

	return UNiagaraFunctionLibrary::SpawnSystemAttached(Definition.NiagaraSystem, MeshComponent, Definition.SocketName, FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::SnapToTarget, bAutoDestroy, true);
}

void URSBaseGameplayAbility_BossPattern::HandlePatternMontageFinished()
{
	// 취소 경로에서는 한 Task가 중단과 종료로 두 번 알릴 수 있으므로 개수가 음수로 내려가지 않게 막습니다
	PendingPatternMontageCount = FMath::Max(0, PendingPatternMontageCount - 1);

	TryFinishPattern();
}

void URSBaseGameplayAbility_BossPattern::TryFinishPattern()
{
	if (!bPatternTimelineFinished || PendingPatternMontageCount > 0 || !IsActive())
	{
		return;
	}

	// EndAbility가 남은 Task를 끝내며 다시 이 경로로 돌아올 수 있으므로 요청을 먼저 내립니다
	bPatternTimelineFinished = false;

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
