// Fill out your copyright notice in the Description page of Project Settings.

#include "RSGameplayAbility_ConcentricRings.h"

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Animation/AnimMontage.h"
#include "Engine/World.h"
#include "RSAttackTelegraphComponent.h"
#include "RSGameplayTags.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

URSGameplayAbility_ConcentricRings::URSGameplayAbility_ConcentricRings()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// 외부에서 이 패턴만 지목해 취소할 수 있게 합니다
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(RSGameplayTags::Ability_Combat_ConcentricRings);
	SetAssetTags(AssetTags);

	// 행동이 잠긴 구간에서는 새 패턴이 시작되지 않게 합니다
	ActivationBlockedTags.AddTag(RSGameplayTags::State_Action_Locked);

	// 에셋 없이도 PIE에서 패턴을 바로 확인할 수 있는 개발용 출발점이며 기획이 정할 값입니다
	constexpr float DefaultInnermostRadius = 600.0f;
	constexpr float DefaultRingThickness = 200.0f;
	constexpr int32 DefaultOuterRingCount = 2;

	// 가장 안쪽은 InnerRadius를 두지 않아 꽉 찬 원이 됩니다. 중심에 구멍이 남으면 거기 서서 패턴 전체를 무시할 수 있습니다
	FRSCombatShape& InnermostRing = Rings.AddDefaulted_GetRef();
	InnermostRing.Type = ERSCombatShapeType::Sphere;
	InnermostRing.Radius = DefaultInnermostRadius;

	for (int32 OuterRingIndex = 0; OuterRingIndex < DefaultOuterRingCount; ++OuterRingIndex)
	{
		FRSCombatShape& Ring = Rings.AddDefaulted_GetRef();
		Ring.Type = ERSCombatShapeType::Sphere;
		Ring.InnerRadius = DefaultInnermostRadius + OuterRingIndex * DefaultRingThickness;
		Ring.Radius = Ring.InnerRadius + DefaultRingThickness;
	}

	// 구조체 기본값은 반응 없음이라 이 패턴이 원하는 넉다운을 지정합니다
	Reaction.Type = ERSHitReactionType::Knockdown;

	// 공격 범위를 중심에서만 보여 주면 비어 있는 안전한 링을 구분할 수 없으므로 링 띠를 채웁니다
	FRSBossPatternNiagaraEntry& FillNiagaraEntry = PatternPresentation.Niagaras.AddDefaulted_GetRef();
	FillNiagaraEntry.Placement = ERSBossPatternNiagaraPlacement::FillHitShape;

	// 스텝마다 안전할 링이며 후보가 하나면 고정 순서가 됩니다
	FRSRingSafeSequence& DefaultSequence = SequencePool.AddDefaulted_GetRef();
	DefaultSequence.SafeRingIndices = { 2, 0, 1 };
}

void URSGameplayAbility_ConcentricRings::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// InstancedPerActor라 인스턴스가 재사용되므로 이전 실행의 상태를 먼저 전부 되돌립니다
	ActiveSequence.Reset();
	RingCenterTransform = FTransform::Identity;
	StepIndex = 0;

	const AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	if (!AvatarActor || Rings.IsEmpty() || SequencePool.IsEmpty())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	// 시퀀스는 여기서 한 번만 확정하고 예고와 실행이 같은 배열을 읽습니다
	// 두 번 뽑으면 예고한 안전 위치와 실제 안전 위치가 갈라집니다
	const int32 PoolIndex = FMath::RandHelper(SequencePool.Num());
	ActiveSequence = SequencePool[PoolIndex].SafeRingIndices;

	if (ActiveSequence.IsEmpty())
	{
		// 설정이 어긋난 상태이므로 판정 디버그 여부와 상관없이 항상 알립니다
		UE_LOG(LogTemp, Warning, TEXT("%s picked an empty ring sequence at pool index %d"), *GetName(), PoolIndex);

		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	// 링 중심도 여기서 한 번만 캡처합니다. 예고 중 보스가 움직여도 판정 자리는 바뀌지 않습니다
	FVector RingCenterLocation = FVector::ZeroVector;
	if (!URSCombatFunctionLibrary::TryGetActorGroundLocation(AvatarActor, RingCenterLocation))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	RingCenterTransform = FTransform(RingCenterLocation);

	// 조기 종료 경로를 모두 지난 뒤에 요청해 실행되지 않을 패턴의 Montage가 재생되지 않게 합니다
	RequestAttackMontage();

	RunCurrentStep();
}

void URSGameplayAbility_ConcentricRings::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 표시는 어빌리티가 아니라 소유자의 컴포넌트에 있으므로 어빌리티가 끝나도 스스로 사라지지 않습니다
	// Super는 재진입 경로에서 아무 일도 하지 않을 수 있으므로 정리를 먼저 합니다
	const AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	if (AvatarActor)
	{
		if (URSAttackTelegraphComponent* TelegraphComp = AvatarActor->FindComponentByClass<URSAttackTelegraphComponent>())
		{
			TelegraphComp->HideAllShapes();
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void URSGameplayAbility_ConcentricRings::RequestAttackMontage()
{
	// Montage는 예고와 타격 시각을 정하지 않지만, 공통 게이트가 수명을 알고 있어 마지막 타격 뒤에도 재생 중인 Montage가 잘리지 않습니다
	PlayPatternMontage(AttackMontage);
}

void URSGameplayAbility_ConcentricRings::RunCurrentStep()
{
	const int32 SequenceLength = ActiveSequence.Num();

	// 마지막 타격이 직접 종료하므로 정상 흐름에서는 도달하지 않는 범위 가드입니다
	if (StepIndex >= SequenceLength * 2)
	{
		FinishPatternWhenMontageEnds();

		return;
	}

	// Avatar가 사라지면 남은 스텝을 진행할 수 없으므로 스텝을 건너뛰지 않고 패턴을 끝냅니다
	const AActor* AvatarActor = CurrentActorInfo ? CurrentActorInfo->AvatarActor.Get() : nullptr;
	if (!AvatarActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s lost its avatar actor at step %d"), *GetName(), StepIndex);

		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	const bool bIsPreviewStep = StepIndex < SequenceLength;
	const int32 SequenceIndex = bIsPreviewStep ? StepIndex : StepIndex - SequenceLength;
	const bool bIsLastStepOfPhase = SequenceIndex == SequenceLength - 1;

	if (bIsPreviewStep)
	{
		PreviewDangerRings(*AvatarActor, SequenceIndex);

		// 마지막 예고가 끝난 뒤에는 플레이어가 자리를 잡을 시간을 줍니다
		ScheduleNextStep(PreviewShowDuration + (bIsLastStepOfPhase ? InterludeDuration : PreviewInterval));

		return;
	}

	StrikeDangerRings(*AvatarActor, SequenceIndex);

	// 마지막 타격 뒤에 대기를 예약하지 않도록 한다
	// 판정은 끝났지만 Montage가 남아 있으면 잘리지 않도록 종료를 미룬다
	if (bIsLastStepOfPhase)
	{
		FinishPatternWhenMontageEnds();

		return;
	}

	ScheduleNextStep(StrikeInterval);
}

void URSGameplayAbility_ConcentricRings::ScheduleNextStep(float DelaySeconds)
{
	// Timer 대신 AbilityTask를 쓰면 어빌리티가 취소될 때 정리가 콘솔 변수가 아니라 Task 수명으로 보장됩니다
	UAbilityTask_WaitDelay* StepDelayTask = UAbilityTask_WaitDelay::WaitDelay(this, FMath::Max(DelaySeconds, UE_KINDA_SMALL_NUMBER));
	StepDelayTask->OnFinish.AddDynamic(this, &ThisClass::HandleStepDelayFinished);
	StepDelayTask->ReadyForActivation();
}

void URSGameplayAbility_ConcentricRings::HandleStepDelayFinished()
{
	++StepIndex;

	RunCurrentStep();
}

bool URSGameplayAbility_ConcentricRings::TryGetDangerRingShapes(int32 SequenceIndex, TArray<FRSCombatShape>& OutDangerRings) const
{
	OutDangerRings.Reset();

	if (!ActiveSequence.IsValidIndex(SequenceIndex))
	{
		return false;
	}

	const int32 SafeRingIndex = ActiveSequence[SequenceIndex];
	if (!Rings.IsValidIndex(SafeRingIndex))
	{
		return false;
	}

	// 안전한 링 하나만 빼고 전부 위험합니다. 링은 겹치지 않으므로 이 목록이 판정 영역을 빠짐없이 한 번씩 덮습니다
	OutDangerRings.Reserve(Rings.Num() - 1);
	for (int32 RingIndex = 0; RingIndex < Rings.Num(); ++RingIndex)
	{
		if (RingIndex == SafeRingIndex)
		{
			continue;
		}

		OutDangerRings.Add(Rings[RingIndex]);
	}

	return true;
}

void URSGameplayAbility_ConcentricRings::PreviewDangerRings(const AActor& AvatarActor, int32 SequenceIndex)
{
	TArray<FRSCombatShape> DangerRings;
	if (!TryGetDangerRingShapes(SequenceIndex, DangerRings))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s previewed step %d but its safe ring index is out of range"), *GetName(), SequenceIndex);

		return;
	}

	// 예고는 표시 컴포넌트에 맡기고 어빌리티는 무엇을 어디에 얼마나 보여줄지만 지시합니다
	// 컴포넌트가 없으면 예고가 보이지 않을 뿐 판정과 진행은 그대로 동작합니다
	if (URSAttackTelegraphComponent* TelegraphComp = AvatarActor.FindComponentByClass<URSAttackTelegraphComponent>())
	{
		FRSTelegraphPresentation Presentation;
		Presentation.HoldDuration = PreviewShowDuration;

		// 안전한 링은 그리지 않습니다. 표시가 공격 범위를 뜻하므로 비어 있는 링이 곧 서야 할 자리입니다
		for (const FRSCombatShape& DangerRing : DangerRings)
		{
			TelegraphComp->ShowShape(DangerRing, RingCenterTransform, Presentation);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("%s has no attack telegraph component, so the preview is invisible"), *GetName());
	}

	if (URSCombatFunctionLibrary::IsHitCheckDebugEnabled())
	{
		UE_LOG(LogTemp, Log, TEXT("%s preview %d/%d safe ring %d"), *GetName(), SequenceIndex + 1, ActiveSequence.Num(), ActiveSequence[SequenceIndex]);
	}
}

void URSGameplayAbility_ConcentricRings::StrikeDangerRings(const AActor& AvatarActor, int32 SequenceIndex)
{
	TArray<FRSCombatShape> DangerRings;
	if (!TryGetDangerRingShapes(SequenceIndex, DangerRings))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s struck step %d but its safe ring index is out of range"), *GetName(), SequenceIndex);

		return;
	}

	// 링이 빈틈과 겹침 없이 이어지고 판정이 반개구간이라 한 대상이 두 링에 동시에 걸리지 않으므로 중복 없이 이어 붙입니다
	TArray<AActor*> HitTargets;
	for (const FRSCombatShape& DangerRing : DangerRings)
	{
		TArray<AActor*> RingTargets;
		URSCombatFunctionLibrary::FindTargetsInShape(&AvatarActor, TargetChannel, DangerRing, RingCenterTransform, RingTargets);

		HitTargets.Append(RingTargets);
	}

	// 링은 빗나가도 바닥이 울려야 하므로 적중 여부와 무관하게 한 스텝의 링들이 판정하는 순간에 재생합니다
	// 영역을 한 번에 넘겨야 Niagara만 링마다 나고 Sound와 카메라 셰이크는 한 번만 울립니다
	PlayPatternPresentation(DangerRings, RingCenterTransform);

	if (URSCombatFunctionLibrary::IsHitCheckDebugEnabled())
	{
		UE_LOG(LogTemp, Log, TEXT("%s strike %d/%d safe ring %d struck %d ring(s) and found %d target(s)"), *GetName(), SequenceIndex + 1, ActiveSequence.Num(), ActiveSequence[SequenceIndex], DangerRings.Num(), HitTargets.Num());
	}

	const float DamageAmount = Damage.GetValueAtLevel(GetAbilityLevel(CurrentSpecHandle, CurrentActorInfo));
	for (AActor* HitTarget : HitTargets)
	{
		ApplyDamageToTarget(HitTarget, DamageEffectClass, DamageAmount);
		URSCombatFunctionLibrary::SendHitReaction(&AvatarActor, HitTarget, Reaction);
	}
}

#if WITH_EDITOR
EDataValidationResult URSGameplayAbility_ConcentricRings::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult ValidationResult = Super::IsDataValid(Context);

	// 시퀀스가 가리킨 링 하나가 안전 지대이므로 링이 하나뿐이면 그 하나가 늘 안전해 아무것도 때리지 않습니다
	if (Rings.Num() < 2)
	{
		Context.AddError(FText::FromString(FString::Printf(TEXT("Rings needs at least 2 entries but has %d. Each step spares one ring, so a single ring would never be attacked."), Rings.Num())));
		ValidationResult = EDataValidationResult::Invalid;
	}

	// 중심에 구멍이 남으면 어느 링이 안전하든 거기 서서 시퀀스 전체를 무시할 수 있습니다
	// 링 바깥의 여유는 아레나 크기를 알아야 판단할 수 있어 여기서 검사하지 않습니다
	if (!Rings.IsEmpty() && !FMath::IsNearlyZero(Rings[0].InnerRadius))
	{
		Context.AddError(FText::FromString(FString::Printf(TEXT("Rings[0].InnerRadius must be 0 but is %.0f. A hole at the center would be permanently safe and would make the whole sequence avoidable by standing still."), Rings[0].InnerRadius)));
		ValidationResult = EDataValidationResult::Invalid;
	}

	for (int32 RingIndex = 0; RingIndex < Rings.Num(); ++RingIndex)
	{
		const FRSCombatShape& Ring = Rings[RingIndex];

		if (Ring.Type != ERSCombatShapeType::Sphere)
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("Rings[%d] must use the Sphere shape."), RingIndex)));
			ValidationResult = EDataValidationResult::Invalid;

			continue;
		}

		FString ShapeValidationError;
		if (!Ring.IsDataValid(&ShapeValidationError))
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("Rings[%d] is invalid: %s"), RingIndex, *ShapeValidationError)));
			ValidationResult = EDataValidationResult::Invalid;

			continue;
		}

		// 링이 겹치거나 벌어지면 한 위치가 두 링에 속하거나 어느 링에도 속하지 않아 자기 위치를 판단할 수 없습니다
		if (RingIndex > 0 && !FMath::IsNearlyEqual(Ring.InnerRadius, Rings[RingIndex - 1].Radius))
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("Rings[%d].InnerRadius (%.0f) must match Rings[%d].Radius (%.0f) so the rings stay ordered without overlaps or gaps."), RingIndex, Ring.InnerRadius, RingIndex - 1, Rings[RingIndex - 1].Radius)));
			ValidationResult = EDataValidationResult::Invalid;
		}
	}

	// 링마다 크기가 달라 채우기 결과도 다르므로 전부 검사하되, 설정 하나가 모든 링에서 같은 오류를 반복하지 않게 첫 실패에서 멈춥니다
	for (const FRSCombatShape& Ring : Rings)
	{
		if (!Ring.IsDataValid())
		{
			continue;
		}

		const EDataValidationResult PresentationResult = ValidatePatternPresentation(Ring, Context);
		ValidationResult = CombineDataValidationResults(ValidationResult, PresentationResult);
		if (PresentationResult == EDataValidationResult::Invalid)
		{
			break;
		}
	}

	if (SequencePool.IsEmpty())
	{
		Context.AddError(FText::FromString(TEXT("SequencePool is empty, so the pattern has no safe ring order to run.")));
		ValidationResult = EDataValidationResult::Invalid;
	}

	for (int32 PoolIndex = 0; PoolIndex < SequencePool.Num(); ++PoolIndex)
	{
		const TArray<int32>& SafeRingIndices = SequencePool[PoolIndex].SafeRingIndices;

		if (SafeRingIndices.IsEmpty())
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("SequencePool[%d] is empty."), PoolIndex)));
			ValidationResult = EDataValidationResult::Invalid;

			continue;
		}

		for (int32 StepPosition = 0; StepPosition < SafeRingIndices.Num(); ++StepPosition)
		{
			if (!Rings.IsValidIndex(SafeRingIndices[StepPosition]))
			{
				Context.AddError(FText::FromString(FString::Printf(TEXT("SequencePool[%d].SafeRingIndices[%d] is %d but Rings has %d entries."), PoolIndex, StepPosition, SafeRingIndices[StepPosition], Rings.Num())));
				ValidationResult = EDataValidationResult::Invalid;
			}
		}
	}

	if (!URSCombatFunctionLibrary::ValidateIntegerDamage(Damage, TEXT("Damage"), Context))
	{
		ValidationResult = EDataValidationResult::Invalid;
	}

	return ValidationResult;
}
#endif
