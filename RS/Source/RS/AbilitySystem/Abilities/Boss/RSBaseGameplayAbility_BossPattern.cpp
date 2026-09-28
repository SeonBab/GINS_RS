// Fill out your copyright notice in the Description page of Project Settings.


#include "RSBaseGameplayAbility_BossPattern.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "RSBossCharacter.h"
#include "RSBossController.h"
#include "RSBossPatternPresentationExecutor.h"
#include "RSBossPhaseComponent.h"
#include "Tasks/RSAbilityTask_BossFacing.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

URSBaseGameplayAbility_BossPattern::URSBaseGameplayAbility_BossPattern()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

#if WITH_EDITOR
EDataValidationResult URSBaseGameplayAbility_BossPattern::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult ValidationResult = Super::IsDataValid(Context);

	if (!HitDefinition.DamageEffectClass)
	{
		Context.AddError(FText::FromString(TEXT("HitDefinition.DamageEffectClass is required.")));
		ValidationResult = EDataValidationResult::Invalid;
	}

	if (!URSCombatFunctionLibrary::ValidateIntegerDamage(HitDefinition.Damage, TEXT("HitDefinition.Damage"), Context))
	{
		ValidationResult = EDataValidationResult::Invalid;
	}

	const FRSHitReactionDefinition& Reaction = HitDefinition.Reaction;
	if (Reaction.Type == ERSHitReactionType::Knockdown
		&& (!FMath::IsFinite(Reaction.KnockbackDistance) || Reaction.KnockbackDistance < 0.0f
			|| !FMath::IsFinite(Reaction.KnockbackHeight) || Reaction.KnockbackHeight < 0.0f
			|| !FMath::IsFinite(Reaction.KnockbackDuration) || Reaction.KnockbackDuration <= 0.0f))
	{
		Context.AddError(FText::FromString(TEXT("HitDefinition.Reaction Knockdown values are outside their supported ranges.")));
		ValidationResult = EDataValidationResult::Invalid;
	}

	return ValidationResult;
}
#endif

void URSBaseGameplayAbility_BossPattern::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	bAnyTargetHit = false;

	// InstancedPerActor라 인스턴스가 재사용되므로 이전 실행의 Montage 게이트 상태를 먼저 되돌립니다
	bPatternTimelineFinished = false;
	bIsInPatternRecovery = false;
	PendingPatternMontageCount = 0;
	PatternMontageTasks.Reset();
	PlayedPatternMontages.Reset();
	EndActiveBossFacing();

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (WindupDuration <= 0.0f)
	{
		BeginPatternTimeline();

		return;
	}

	UAbilityTask_WaitDelay* WindupTask = UAbilityTask_WaitDelay::WaitDelay(this, WindupDuration);
	if (!WindupTask)
	{
		// 대기를 만들지 못했다고 패턴 자체를 버리면 보스가 아무것도 하지 않는 차례가 생기므로 선딜만 포기합니다
		BeginPatternTimeline();

		return;
	}

	WindupTask->OnFinish.AddDynamic(this, &ThisClass::HandleWindupFinished);
	WindupTask->ReadyForActivation();
}

void URSBaseGameplayAbility_BossPattern::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	EndActiveBossFacing();

	// Task를 끝내면 Montage가 중단되어 완료 Callback이 되돌아올 수 있으므로 게이트를 먼저 닫고 목록을 비웁니다
	TArray<TObjectPtr<UAbilityTask_PlayMontageAndWait>> TasksToEnd = MoveTemp(PatternMontageTasks);
	PatternMontageTasks.Reset();
	PendingPatternMontageCount = 0;
	bPatternTimelineFinished = false;
	bIsInPatternRecovery = false;

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

void URSBaseGameplayAbility_BossPattern::ApplyPatternHitToTarget(AActor* TargetActor)
{
	ApplyDamageToTarget(TargetActor, HitDefinition.DamageEffectClass, GetPatternDamageAmount());
	URSCombatFunctionLibrary::SendHitReaction(GetAvatarActorFromActorInfo(), TargetActor, HitDefinition.Reaction);
}

void URSBaseGameplayAbility_BossPattern::ApplyPatternHitToTarget(AActor* TargetActor, const FVector& KnockbackDirection)
{
	ApplyDamageToTarget(TargetActor, HitDefinition.DamageEffectClass, GetPatternDamageAmount());
	URSCombatFunctionLibrary::SendHitReactionWithKnockbackDirection(GetAvatarActorFromActorInfo(), TargetActor, HitDefinition.Reaction, KnockbackDirection);
}

FRSBossPatternHitSpec URSBaseGameplayAbility_BossPattern::MakePatternHitSpec() const
{
	FRSBossPatternHitSpec HitSpec;
	HitSpec.DamageAmount = GetPatternDamageAmount();
	HitSpec.DamageEffectClass = HitDefinition.DamageEffectClass;
	HitSpec.Reaction = HitDefinition.Reaction;

	return HitSpec;
}

float URSBaseGameplayAbility_BossPattern::GetPatternDamageAmount() const
{
	return GetAbilityLevel() > 0 ? HitDefinition.Damage.GetValueAtLevel(GetAbilityLevel()) : HitDefinition.Damage.GetValueAtLevel(1.0f);
}

bool URSBaseGameplayAbility_BossPattern::TryGetBossContext(ARSBossCharacter*& OutBossCharacter, ARSBossController*& OutBossController) const
{
	OutBossCharacter = CurrentActorInfo ? Cast<ARSBossCharacter>(CurrentActorInfo->AvatarActor.Get()) : nullptr;
	OutBossController = OutBossCharacter ? Cast<ARSBossController>(OutBossCharacter->GetController()) : nullptr;

	return OutBossCharacter && OutBossController;
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

URSAbilityTask_BossFacing* URSBaseGameplayAbility_BossPattern::CreateBossFacingTask(const FRSBossFacingRequest& Request)
{
	EndActiveBossFacing();
	ActiveBossFacingTask = URSAbilityTask_BossFacing::CreateBossFacingTask(this, Request);

	return ActiveBossFacingTask;
}

void URSBaseGameplayAbility_BossPattern::EndActiveBossFacing()
{
	if (!ActiveBossFacingTask)
	{
		return;
	}

	URSAbilityTask_BossFacing* FacingTask = ActiveBossFacingTask;
	ActiveBossFacingTask = nullptr;
	FacingTask->EndTask();
}

void URSBaseGameplayAbility_BossPattern::PlayPatternPresentation(const TArray<FTransform>& NiagaraTransforms, const FVector& SoundLocation) const
{
	PlayPatternPresentationInternal(TArrayView<const FRSCombatShape>(), NiagaraTransforms, SoundLocation);
}

void URSBaseGameplayAbility_BossPattern::PlayPatternPresentation(const FTransform& PresentationTransform) const
{
	PlayPatternPresentation(TArray<FTransform>({ PresentationTransform }), PresentationTransform.GetLocation());
}

void URSBaseGameplayAbility_BossPattern::PlayPatternPresentation(const FRSCombatShape& HitShape, const TArray<FTransform>& ShapeTransforms, const FVector& SoundLocation) const
{
	PlayPatternPresentationInternal(TArrayView<const FRSCombatShape>(&HitShape, 1), ShapeTransforms, SoundLocation);
}

void URSBaseGameplayAbility_BossPattern::PlayPatternPresentation(const FRSCombatShape& HitShape, const FTransform& ShapeTransform) const
{
	PlayPatternPresentation(HitShape, TArray<FTransform>({ ShapeTransform }), ShapeTransform.GetLocation());
}

void URSBaseGameplayAbility_BossPattern::PlayPatternPresentation(TArrayView<const FRSCombatShape> HitShapes, const FTransform& ShapeTransform) const
{
	PlayPatternPresentationInternal(HitShapes, TArrayView<const FTransform>(&ShapeTransform, 1), ShapeTransform.GetLocation());
}

void URSBaseGameplayAbility_BossPattern::PlayPatternPresentationInternal(TArrayView<const FRSCombatShape> HitShapes, TArrayView<const FTransform> PresentationTransforms, const FVector& SoundLocation) const
{
	USkeletalMeshComponent* MeshComponent = CurrentActorInfo ? CurrentActorInfo->SkeletalMeshComponent.Get() : nullptr;
	FRSBossPatternPresentationExecutor::Play(this, MeshComponent, PatternPresentation, HitShapes, PresentationTransforms, SoundLocation);
}

#if WITH_EDITOR
EDataValidationResult URSBaseGameplayAbility_BossPattern::ValidatePatternPresentation(const FRSCombatShape& HitShape, FDataValidationContext& Context) const
{
	return FRSBossPatternPresentationExecutor::Validate(PatternPresentation, HitShape, Context);
}
#endif

UNiagaraComponent* URSBaseGameplayAbility_BossPattern::SpawnNiagaraFromDefinition(const FRSNiagaraSpawnDefinition& Definition, const FTransform& WorldTransform, bool bAutoDestroy) const
{
	// 생성 규칙은 보스 패턴만의 것이 아니므로 공용 라이브러리가 소유하고 여기서는 소켓 기준 Mesh만 정합니다
	USkeletalMeshComponent* MeshComponent = CurrentActorInfo ? CurrentActorInfo->SkeletalMeshComponent.Get() : nullptr;

	return FRSBossPatternPresentationExecutor::SpawnNiagaraFromDefinition(this, MeshComponent, Definition, WorldTransform, bAutoDestroy);
}

void URSBaseGameplayAbility_BossPattern::HandlePatternMontageFinished()
{
	// 취소 경로에서는 한 Task가 중단과 종료로 두 번 알릴 수 있으므로 개수가 음수로 내려가지 않게 막습니다
	PendingPatternMontageCount = FMath::Max(0, PendingPatternMontageCount - 1);

	TryFinishPattern();
}

void URSBaseGameplayAbility_BossPattern::HandleWindupFinished()
{
	BeginPatternTimeline();
}

void URSBaseGameplayAbility_BossPattern::HandleRecoveryFinished()
{
	if (!IsActive())
	{
		return;
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void URSBaseGameplayAbility_BossPattern::TryFinishPattern()
{
	if (!bPatternTimelineFinished || PendingPatternMontageCount > 0 || !IsActive() || bIsInPatternRecovery)
	{
		return;
	}

	// EndAbility가 남은 Task를 끝내며 다시 이 경로로 돌아올 수 있으므로 요청을 먼저 내립니다
	bPatternTimelineFinished = false;

	if (RecoveryDuration > 0.0f)
	{
		if (UAbilityTask_WaitDelay* RecoveryTask = UAbilityTask_WaitDelay::WaitDelay(this, RecoveryDuration))
		{
			// 대기 중에 Montage 신호가 다시 들어와도 같은 대기를 겹쳐 시작하지 않게 막습니다
			bIsInPatternRecovery = true;

			RecoveryTask->OnFinish.AddDynamic(this, &ThisClass::HandleRecoveryFinished);
			RecoveryTask->ReadyForActivation();

			return;
		}
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
