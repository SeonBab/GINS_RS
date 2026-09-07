// Fill out your copyright notice in the Description page of Project Settings.

#include "RSGameplayAbility_ConcentricRings.h"

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
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
	constexpr int32 DefaultRingCount = 3;
	constexpr float DefaultInnermostRadius = 400.0f;
	constexpr float DefaultRingThickness = 200.0f;

	for (int32 RingIndex = 0; RingIndex < DefaultRingCount; ++RingIndex)
	{
		FRSCombatShape& Ring = Rings.AddDefaulted_GetRef();
		Ring.Type = ERSCombatShapeType::Sphere;
		Ring.InnerRadius = DefaultInnermostRadius + RingIndex * DefaultRingThickness;
		Ring.Radius = Ring.InnerRadius + DefaultRingThickness;
	}

	// 구조체 기본값은 반응 없음이라 이 패턴이 원하는 넉다운을 지정합니다
	Reaction.Type = ERSHitReactionType::Knockdown;

	// 후보가 하나면 고정 순서가 됩니다
	FRSRingAttackSequence& DefaultSequence = SequencePool.AddDefaulted_GetRef();
	DefaultSequence.RingIndices = { 2, 0, 1 };
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
	// 두 번 뽑으면 예고한 순서와 때리는 순서가 갈라집니다
	const int32 PoolIndex = FMath::RandHelper(SequencePool.Num());
	ActiveSequence = SequencePool[PoolIndex].RingIndices;

	if (ActiveSequence.IsEmpty())
	{
		// 설정이 어긋난 상태이므로 판정 디버그 여부와 상관없이 항상 알립니다
		UE_LOG(LogTemp, Warning, TEXT("%s picked an empty ring sequence at pool index %d"), *GetName(), PoolIndex);

		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	// 링 중심도 여기서 한 번만 캡처합니다. 예고 중 보스가 움직여도 판정 자리는 바뀌지 않습니다
	// 액터 위치는 캡슐 중심이라 그대로 쓰면 표시가 공중에 떠서 실제 판정 경계와 어긋나 보입니다
	const FVector RingCenterLocation = AvatarActor->GetActorLocation() - FVector(0.0f, 0.0f, AvatarActor->GetSimpleCollisionHalfHeight());
	RingCenterTransform = FTransform(RingCenterLocation);

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

void URSGameplayAbility_ConcentricRings::RunCurrentStep()
{
	const int32 SequenceLength = ActiveSequence.Num();

	// 마지막 타격이 직접 종료하므로 정상 흐름에서는 도달하지 않는 범위 가드입니다
	if (StepIndex >= SequenceLength * 2)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);

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
		PreviewRing(*AvatarActor, SequenceIndex);

		// 마지막 예고가 끝난 뒤에는 플레이어가 자리를 잡을 시간을 줍니다
		ScheduleNextStep(PreviewShowDuration + (bIsLastStepOfPhase ? InterludeDuration : PreviewInterval));

		return;
	}

	StrikeRing(*AvatarActor, SequenceIndex);

	// 마지막 타격 뒤에 대기를 예약하지 않도록 한다
	if (bIsLastStepOfPhase)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);

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

const FRSCombatShape* URSGameplayAbility_ConcentricRings::GetRingShape(int32 SequenceIndex) const
{
	if (!ActiveSequence.IsValidIndex(SequenceIndex))
	{
		return nullptr;
	}

	const int32 RingIndex = ActiveSequence[SequenceIndex];

	return Rings.IsValidIndex(RingIndex) ? &Rings[RingIndex] : nullptr;
}

void URSGameplayAbility_ConcentricRings::PreviewRing(const AActor& AvatarActor, int32 SequenceIndex)
{
	const FRSCombatShape* RingShape = GetRingShape(SequenceIndex);
	if (!RingShape)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s previewed step %d but its ring index is out of range"), *GetName(), SequenceIndex);

		return;
	}

	// 예고는 표시 컴포넌트에 맡기고 어빌리티는 무엇을 어디에 얼마나 보여줄지만 지시합니다
	// 컴포넌트가 없으면 예고가 보이지 않을 뿐 판정과 진행은 그대로 동작합니다
	if (URSAttackTelegraphComponent* TelegraphComp = AvatarActor.FindComponentByClass<URSAttackTelegraphComponent>())
	{
		FRSTelegraphPresentation Presentation;
		Presentation.HoldDuration = PreviewShowDuration;

		TelegraphComp->ShowShape(*RingShape, RingCenterTransform, Presentation);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("%s has no attack telegraph component, so the preview is invisible"), *GetName());
	}

	if (URSCombatFunctionLibrary::IsHitCheckDebugEnabled())
	{
		UE_LOG(LogTemp, Log, TEXT("%s preview %d/%d ring %d"), *GetName(), SequenceIndex + 1, ActiveSequence.Num(), ActiveSequence[SequenceIndex]);
	}
}

void URSGameplayAbility_ConcentricRings::StrikeRing(const AActor& AvatarActor, int32 SequenceIndex)
{
	const FRSCombatShape* RingShape = GetRingShape(SequenceIndex);
	if (!RingShape)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s struck step %d but its ring index is out of range"), *GetName(), SequenceIndex);

		return;
	}

	TArray<AActor*> HitTargets;
	URSCombatFunctionLibrary::FindTargetsInShape(&AvatarActor, TargetChannel, *RingShape, RingCenterTransform, HitTargets);

	if (URSCombatFunctionLibrary::IsHitCheckDebugEnabled())
	{
		UE_LOG(LogTemp, Log, TEXT("%s strike %d/%d ring %d found %d target(s)"), *GetName(), SequenceIndex + 1, ActiveSequence.Num(), ActiveSequence[SequenceIndex], HitTargets.Num());
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

	if (Rings.IsEmpty())
	{
		Context.AddError(FText::FromString(TEXT("Rings is empty, so the pattern has no area to attack.")));
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

		if (Ring.InnerRadius <= 0.0f || Ring.InnerRadius >= Ring.Radius)
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("Rings[%d] needs 0 < InnerRadius (%.0f) < Radius (%.0f). Every ring is a donut; the center hole is outside this pattern."), RingIndex, Ring.InnerRadius, Ring.Radius)));
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

	if (SequencePool.IsEmpty())
	{
		Context.AddError(FText::FromString(TEXT("SequencePool is empty, so the pattern has no attack order to run.")));
		ValidationResult = EDataValidationResult::Invalid;
	}

	for (int32 PoolIndex = 0; PoolIndex < SequencePool.Num(); ++PoolIndex)
	{
		const TArray<int32>& RingIndices = SequencePool[PoolIndex].RingIndices;

		if (RingIndices.IsEmpty())
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("SequencePool[%d] is empty."), PoolIndex)));
			ValidationResult = EDataValidationResult::Invalid;

			continue;
		}

		for (int32 StepPosition = 0; StepPosition < RingIndices.Num(); ++StepPosition)
		{
			if (!Rings.IsValidIndex(RingIndices[StepPosition]))
			{
				Context.AddError(FText::FromString(FString::Printf(TEXT("SequencePool[%d].RingIndices[%d] is %d but Rings has %d entries."), PoolIndex, StepPosition, RingIndices[StepPosition], Rings.Num())));
				ValidationResult = EDataValidationResult::Invalid;
			}
		}
	}

	return ValidationResult;
}
#endif
