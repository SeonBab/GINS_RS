// Fill out your copyright notice in the Description page of Project Settings.


#include "RSBaseGameplayAbility_BossPattern.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "RSBossPhaseComponent.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace
{
	// 칸 하나마다 Niagara가 하나씩 생기므로 설정 실수로 한 번의 연출이 렌더링을 압도하지 않게 막을 상한입니다
	constexpr int32 RSMaxNiagaraFillCount = 256;
}

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
	for (const FRSBossPatternNiagaraEntry& NiagaraEntry : PatternPresentation.Niagaras)
	{
		if (!NiagaraEntry.Niagara.NiagaraSystem)
		{
			continue;
		}

		// 형상을 넘기지 않는 패턴에서는 채울 범위가 없으므로 넘겨받은 지점에서 그대로 재생합니다
		if (NiagaraEntry.Placement != ERSBossPatternNiagaraPlacement::FillHitShape || HitShapes.IsEmpty())
		{
			for (const FTransform& PresentationTransform : PresentationTransforms)
			{
				SpawnNiagaraFromDefinition(NiagaraEntry.Niagara, PresentationTransform, true);
			}

			continue;
		}

		// 형상과 지점을 교차해 채우므로 형상 하나를 여러 지점에 두는 패턴과 한 지점에 형상 여러 개를 겹치는 패턴이 같은 경로를 씁니다
		for (const FRSCombatShape& HitShape : HitShapes)
		{
			for (const FTransform& PresentationTransform : PresentationTransforms)
			{
				TArray<FTransform> FillTransforms;
				if (!URSCombatFunctionLibrary::BuildShapeFillTransforms(HitShape, PresentationTransform, NiagaraEntry.FillSpacing, FillTransforms))
				{
					// Data Validation이 막지 못한 조합이라도 그 영역의 연출까지 사라지면 안 되므로 기준 지점 하나로 되돌립니다
					SpawnNiagaraFromDefinition(NiagaraEntry.Niagara, PresentationTransform, true);

					continue;
				}

				for (const FTransform& FillTransform : FillTransforms)
				{
					SpawnNiagaraFromDefinition(NiagaraEntry.Niagara, FillTransform, true);
				}
			}
		}
	}

	// 광역 패턴은 Niagara가 여러 지점에서 나지만 소리까지 겹쳐 울리면 한 번의 공격으로 들리지 않습니다
	if (PatternPresentation.Sound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, PatternPresentation.Sound, SoundLocation);
	}

	URSCombatFunctionLibrary::PlayCameraShake(this, PatternPresentation.CameraShake);
}

#if WITH_EDITOR
EDataValidationResult URSBaseGameplayAbility_BossPattern::ValidatePatternPresentation(const FRSCombatShape& HitShape, FDataValidationContext& Context) const
{
	EDataValidationResult ValidationResult = EDataValidationResult::Valid;

	// 항목마다 칸을 따로 채우므로 한 항목이 상한을 넘지 않아도 합쳐서 넘을 수 있습니다
	int32 TotalFillCount = 0;
	for (int32 EntryIndex = 0; EntryIndex < PatternPresentation.Niagaras.Num(); ++EntryIndex)
	{
		const FRSBossPatternNiagaraEntry& NiagaraEntry = PatternPresentation.Niagaras[EntryIndex];
		if (NiagaraEntry.Placement != ERSBossPatternNiagaraPlacement::FillHitShape)
		{
			continue;
		}

		// 채우기는 칸마다 다른 월드 위치를 써야 의미가 있고, 소켓 기준 생성은 모든 칸을 한 지점에 겹쳐 버립니다
		if (NiagaraEntry.Niagara.NiagaraSystem && NiagaraEntry.Niagara.SpawnMode != ERSNiagaraSpawnMode::WorldTransform)
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("PatternPresentation.Niagaras[%d] must use the WorldTransform spawn mode so the fill can place one system per cell."), EntryIndex)));
			ValidationResult = EDataValidationResult::Invalid;
		}

		if (!FMath::IsFinite(NiagaraEntry.FillSpacing) || NiagaraEntry.FillSpacing <= 0.0f)
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("PatternPresentation.Niagaras[%d].FillSpacing must be finite and greater than zero."), EntryIndex)));
			ValidationResult = EDataValidationResult::Invalid;

			continue;
		}

		if (!HitShape.IsDataValid())
		{
			continue;
		}

		// 생성 개수는 형상과 간격만으로 정해지므로 런타임에 칸을 버리지 않고 여기서 막습니다
		TArray<FTransform> FillTransformProbe;
		if (!URSCombatFunctionLibrary::BuildShapeFillTransforms(HitShape, FTransform::Identity, NiagaraEntry.FillSpacing, FillTransformProbe))
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("PatternPresentation.Niagaras[%d].FillSpacing cannot fill the attack shape. Widen the spacing, or use the Ability Defined placement for a shape the fill does not support."), EntryIndex)));
			ValidationResult = EDataValidationResult::Invalid;

			continue;
		}

		TotalFillCount += FillTransformProbe.Num();
	}

	if (TotalFillCount > RSMaxNiagaraFillCount)
	{
		Context.AddError(FText::FromString(FString::Printf(TEXT("The attack shape and PatternPresentation.Niagaras would spawn %d Niagara systems in total and the limit is %d. Increase the fill spacing, reduce the attack shape, or remove a filled entry."), TotalFillCount, RSMaxNiagaraFillCount)));
		ValidationResult = EDataValidationResult::Invalid;
	}

	return ValidationResult;
}
#endif

UNiagaraComponent* URSBaseGameplayAbility_BossPattern::SpawnNiagaraFromDefinition(const FRSNiagaraSpawnDefinition& Definition, const FTransform& WorldTransform, bool bAutoDestroy) const
{
	// 생성 규칙은 보스 패턴만의 것이 아니므로 공용 라이브러리가 소유하고 여기서는 소켓 기준 Mesh만 정합니다
	USkeletalMeshComponent* MeshComponent = CurrentActorInfo ? CurrentActorInfo->SkeletalMeshComponent.Get() : nullptr;

	return URSCombatFunctionLibrary::SpawnNiagaraFromDefinition(this, MeshComponent, Definition, WorldTransform, bAutoDestroy);
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
