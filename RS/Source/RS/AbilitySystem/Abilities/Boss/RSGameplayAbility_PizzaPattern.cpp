#include "RSGameplayAbility_PizzaPattern.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "Combat/RSPizzaPatternMath.h"
#include "RSAttackTelegraphComponent.h"
#include "RSGameplayTags.h"
#include "Tasks/RSAbilityTask_WaitTelegraphFill.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

bool FRSPizzaPatternDefinition::IsDataValid(FString* OutValidationError) const
{
	if (OutValidationError)
	{
		OutValidationError->Reset();
	}

	auto SetValidationError = [OutValidationError](const TCHAR* ErrorMessage)
		{
			if (OutValidationError)
			{
				*OutValidationError = ErrorMessage;
			}
		};

	if (SliceCount < 2)
	{
		SetValidationError(TEXT("SliceCount must be at least 2."));

		return false;
	}

	if (ExplosionCount < 1)
	{
		SetValidationError(TEXT("ExplosionCount must be at least 1."));

		return false;
	}

	if (!FMath::IsFinite(OuterRadius) || OuterRadius <= 0.0f)
	{
		SetValidationError(TEXT("OuterRadius must be finite and greater than zero."));

		return false;
	}

	if (!FMath::IsFinite(AttackStartDelay) || AttackStartDelay < 0.0f)
	{
		SetValidationError(TEXT("AttackStartDelay must be finite and non-negative."));

		return false;
	}

	if (!FMath::IsFinite(TelegraphDuration) || TelegraphDuration <= 0.0f)
	{
		SetValidationError(TEXT("TelegraphDuration must be finite and greater than zero."));

		return false;
	}

	FString TelegraphLeadValidationError;
	if (!TelegraphLeadTime.IsDataValid(TelegraphDuration, &TelegraphLeadValidationError))
	{
		SetValidationError(*TelegraphLeadValidationError);

		return false;
	}

	const float DamageValue = Damage.GetValueAtLevel(1.0f);
	constexpr float DamageIntegerTolerance = 0.01f;
	if (!FMath::IsFinite(DamageValue) || DamageValue < 0.0f || !FMath::IsNearlyEqual(DamageValue, FMath::RoundToFloat(DamageValue), DamageIntegerTolerance))
	{
		SetValidationError(TEXT("Damage must evaluate to a finite non-negative integer at level 1."));

		return false;
	}

	return true;
}

URSGameplayAbility_PizzaPattern::URSGameplayAbility_PizzaPattern()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	ActivationBlockedTags.AddTag(RSGameplayTags::State_Action_Locked);
}

void URSGameplayAbility_PizzaPattern::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	LockedPatternTransform = FTransform::Identity;
	ActiveSliceTransforms.Reset();
	ActiveTelegraphHandles.Reset();
	HitActors.Reset();
	CurrentExplosionIndex = 0;
	MontageTask = nullptr;
	AttackStartDelayTask = nullptr;
	TelegraphFillTask = nullptr;
	bIsCleaningUp = false;

	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid() || !PizzaPatternDefinition.IsDataValid() || !DamageEffectClass)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	RequestAttackMontage();

	if (PizzaPatternDefinition.AttackStartDelay <= 0.0f)
	{
		HandleAttackStartDelayFinished();

		return;
	}

	AttackStartDelayTask = UAbilityTask_WaitDelay::WaitDelay(this, PizzaPatternDefinition.AttackStartDelay);
	AttackStartDelayTask->OnFinish.AddDynamic(this, &ThisClass::HandleAttackStartDelayFinished);
	AttackStartDelayTask->ReadyForActivation();
}

void URSGameplayAbility_PizzaPattern::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (bIsCleaningUp)
	{
		return;
	}

	bIsCleaningUp = true;
	if (AttackStartDelayTask)
	{
		AttackStartDelayTask->EndTask();
		AttackStartDelayTask = nullptr;
	}

	if (TelegraphFillTask)
	{
		TelegraphFillTask->EndTask();
		TelegraphFillTask = nullptr;
	}

	HideActiveTelegraphs();
	EndAnimationGameplayStatesForMontage(ActorInfo, AttackMontage);

	LockedPatternTransform = FTransform::Identity;
	ActiveSliceTransforms.Reset();
	HitActors.Reset();
	CurrentExplosionIndex = 0;
	MontageTask = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	bIsCleaningUp = false;
}

void URSGameplayAbility_PizzaPattern::RequestAttackMontage()
{
	if (!AttackMontage)
	{
		return;
	}

	// Montage Task의 결과를 공격 흐름에 연결하지 않아 재생 실패와 중단이 패턴을 취소하지 않게 합니다
	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, AttackMontage);
	MontageTask->ReadyForActivation();
}

void URSGameplayAbility_PizzaPattern::HandleAttackStartDelayFinished()
{
	AttackStartDelayTask = nullptr;
	if (bIsCleaningUp || !IsActive())
	{
		return;
	}

	if (!TryCaptureLockedPatternTransform())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	BeginExplosionTelegraph();
}

bool URSGameplayAbility_PizzaPattern::TryCaptureLockedPatternTransform()
{
	const ACharacter* Character = CurrentActorInfo ? Cast<ACharacter>(CurrentActorInfo->AvatarActor.Get()) : nullptr;
	const UCapsuleComponent* CapsuleComp = Character ? Character->GetCapsuleComponent() : nullptr;
	if (!Character || !CapsuleComp)
	{
		return false;
	}

	return RSPizzaPatternMath::TryCalculateLockedPatternTransform(CapsuleComp->GetComponentTransform(), CapsuleComp->GetScaledCapsuleHalfHeight(), Character->GetActorForwardVector(), LockedPatternTransform);
}

void URSGameplayAbility_PizzaPattern::BeginExplosionTelegraph()
{
	if (bIsCleaningUp || !IsActive() || CurrentExplosionIndex >= PizzaPatternDefinition.ExplosionCount)
	{
		return;
	}

	AActor* AvatarActor = CurrentActorInfo ? CurrentActorInfo->AvatarActor.Get() : nullptr;
	URSAttackTelegraphComponent* TelegraphComp = AvatarActor ? AvatarActor->FindComponentByClass<URSAttackTelegraphComponent>() : nullptr;
	if (!AvatarActor || !TelegraphComp || !RSPizzaPatternMath::TryBuildExplosionSliceTransforms(LockedPatternTransform, PizzaPatternDefinition.SliceCount, CurrentExplosionIndex, ActiveSliceTransforms))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	FRSCombatShape SliceShape;
	SliceShape.Type = ERSCombatShapeType::Cone;
	SliceShape.Range = PizzaPatternDefinition.OuterRadius;
	SliceShape.Angle = RSPizzaPatternMath::CalculateSliceAngleDegrees(PizzaPatternDefinition.SliceCount);

	ActiveTelegraphHandles.Reset();
	ActiveTelegraphHandles.Reserve(ActiveSliceTransforms.Num());
	for (const FTransform& SliceTransform : ActiveSliceTransforms)
	{
		const int32 TelegraphHandle = TelegraphComp->ShowShapeWithExternalFill(SliceShape, SliceTransform);
		if (TelegraphHandle == INDEX_NONE)
		{
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

			return;
		}

		ActiveTelegraphHandles.Add(TelegraphHandle);
	}

	TelegraphFillTask = URSAbilityTask_WaitTelegraphFill::WaitTelegraphFill(this, PizzaPatternDefinition.TelegraphDuration);
	TelegraphFillTask->OnProgress.AddDynamic(this, &ThisClass::HandleTelegraphFillUpdated);
	TelegraphFillTask->OnFinished.AddDynamic(this, &ThisClass::HandleTelegraphFillFinished);
	TelegraphFillTask->ReadyForActivation();
}

void URSGameplayAbility_PizzaPattern::HandleTelegraphFillUpdated(float Progress)
{
	if (bIsCleaningUp || !IsActive())
	{
		return;
	}

	// Task는 폭발까지의 진행률을 전달하므로 표시의 두 시점은 선행 시간을 반영해 다시 계산합니다
	const float TelegraphDuration = PizzaPatternDefinition.TelegraphDuration;
	const FRSTelegraphLeadTime& TelegraphLeadTime = PizzaPatternDefinition.TelegraphLeadTime;
	const float ElapsedTime = Progress * TelegraphDuration;
	if (ElapsedTime >= TelegraphLeadTime.GetHideTime(TelegraphDuration))
	{
		if (!ActiveTelegraphHandles.IsEmpty())
		{
			HideActiveTelegraphs();
		}

		return;
	}

	const float FillDuration = TelegraphLeadTime.GetFillDuration(TelegraphDuration);
	const float Fill = FillDuration > 0.0f ? FMath::Min(ElapsedTime / FillDuration, 1.0f) : 1.0f;
	if (!SetActiveTelegraphFill(Fill))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

void URSGameplayAbility_PizzaPattern::HandleTelegraphFillFinished()
{
	TelegraphFillTask = nullptr;
	if (bIsCleaningUp || !IsActive())
	{
		return;
	}

	// 표시는 진행률 갱신 경로에서 폭발보다 먼저 사라지므로 여기서는 남은 것만 회수합니다
	HideActiveTelegraphs();
	if (!ExecuteCurrentExplosion())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}
	PlayExplosionPresentation();

	++CurrentExplosionIndex;
	if (CurrentExplosionIndex >= PizzaPatternDefinition.ExplosionCount)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);

		return;
	}

	BeginExplosionTelegraph();
}

bool URSGameplayAbility_PizzaPattern::ExecuteCurrentExplosion()
{
	AActor* AvatarActor = CurrentActorInfo ? CurrentActorInfo->AvatarActor.Get() : nullptr;
	if (!AvatarActor)
	{
		return false;
	}

	const float DamageAmount = PizzaPatternDefinition.Damage.GetValueAtLevel(GetAbilityLevel(CurrentSpecHandle, CurrentActorInfo));
	if (!FMath::IsFinite(DamageAmount) || DamageAmount < 0.0f)
	{
		return false;
	}

	FRSCombatShape CandidateShape;
	CandidateShape.Type = ERSCombatShapeType::Sphere;
	CandidateShape.Radius = PizzaPatternDefinition.OuterRadius;
	CandidateShape.InnerRadius = 0.0f;

	TArray<AActor*> CandidateTargets;
	URSCombatFunctionLibrary::FindTargetsInShape(AvatarActor, TargetChannel, CandidateShape, LockedPatternTransform, CandidateTargets);

	HitActors.Reset();
	for (AActor* CandidateTarget : CandidateTargets)
	{
		const TWeakObjectPtr<AActor> TargetPointer(CandidateTarget);
		if (!CandidateTarget || HitActors.Contains(TargetPointer)
			|| !RSPizzaPatternMath::IsLocationInExplosionGroup(LockedPatternTransform, PizzaPatternDefinition.SliceCount, CurrentExplosionIndex, CandidateTarget->GetActorLocation()))
		{
			continue;
		}

		HitActors.Add(TargetPointer);
		ApplyDamageToTarget(CandidateTarget, DamageEffectClass, DamageAmount);
		URSCombatFunctionLibrary::SendHitReaction(AvatarActor, CandidateTarget, PizzaPatternDefinition.Reaction);
	}

	return true;
}

void URSGameplayAbility_PizzaPattern::PlayExplosionPresentation() const
{
	if (ExplosionNiagara)
	{
		for (const FTransform& SliceTransform : ActiveSliceTransforms)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, ExplosionNiagara, SliceTransform.GetLocation(), SliceTransform.Rotator());
		}
	}

	if (ExplosionSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ExplosionSound, LockedPatternTransform.GetLocation());
	}
}

void URSGameplayAbility_PizzaPattern::HideActiveTelegraphs()
{
	AActor* AvatarActor = CurrentActorInfo ? CurrentActorInfo->AvatarActor.Get() : nullptr;
	URSAttackTelegraphComponent* TelegraphComp = AvatarActor ? AvatarActor->FindComponentByClass<URSAttackTelegraphComponent>() : nullptr;
	if (TelegraphComp)
	{
		for (int32 TelegraphHandle : ActiveTelegraphHandles)
		{
			TelegraphComp->HideShape(TelegraphHandle);
		}
	}

	ActiveTelegraphHandles.Reset();
}

bool URSGameplayAbility_PizzaPattern::SetActiveTelegraphFill(float Fill) const
{
	const AActor* AvatarActor = CurrentActorInfo ? CurrentActorInfo->AvatarActor.Get() : nullptr;
	URSAttackTelegraphComponent* TelegraphComp = AvatarActor ? AvatarActor->FindComponentByClass<URSAttackTelegraphComponent>() : nullptr;
	if (!TelegraphComp || ActiveTelegraphHandles.IsEmpty())
	{
		return false;
	}

	for (int32 TelegraphHandle : ActiveTelegraphHandles)
	{
		if (!TelegraphComp->SetExternalFill(TelegraphHandle, Fill))
		{
			return false;
		}
	}

	return true;
}

#if WITH_EDITOR
EDataValidationResult URSGameplayAbility_PizzaPattern::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult ValidationResult = Super::IsDataValid(Context);

	FString ValidationError;
	if (!PizzaPatternDefinition.IsDataValid(&ValidationError))
	{
		Context.AddError(FText::FromString(FString::Printf(TEXT("PizzaPatternDefinition is invalid: %s"), *ValidationError)));
		ValidationResult = EDataValidationResult::Invalid;
	}

	if (!DamageEffectClass)
	{
		Context.AddError(FText::FromString(TEXT("DamageEffectClass is required.")));
		ValidationResult = EDataValidationResult::Invalid;
	}

	const FGameplayTagContainer& AssetTags = GetAssetTags();
	const bool bHasOneExplosionTag = AssetTags.HasTagExact(RSGameplayTags::Ability_Combat_PizzaPattern_OneExplosion);
	const bool bHasTwoExplosionsTag = AssetTags.HasTagExact(RSGameplayTags::Ability_Combat_PizzaPattern_TwoExplosions);
	if (bHasOneExplosionTag == bHasTwoExplosionsTag)
	{
		Context.AddError(FText::FromString(TEXT("Pizza Pattern Ability must have exactly one OneExplosion or TwoExplosions leaf AssetTag.")));
		ValidationResult = EDataValidationResult::Invalid;
	}
	else if ((bHasOneExplosionTag && PizzaPatternDefinition.ExplosionCount != 1)
		|| (bHasTwoExplosionsTag && PizzaPatternDefinition.ExplosionCount != 2))
	{
		Context.AddError(FText::FromString(TEXT("ExplosionCount must exactly match the OneExplosion or TwoExplosions Pizza Pattern leaf AssetTag.")));
		ValidationResult = EDataValidationResult::Invalid;
	}

	return ValidationResult;
}
#endif
