// Fill out your copyright notice in the Description page of Project Settings.

#include "RSGameplayAbility_RandomFallingRocks.h"

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "Tasks/RSAbilityTask_PendingFallingRock.h"
#include "Combat/RSRandomPointSampling.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

bool FRSRandomFallingRocksDefinition::IsDataValid(FString* OutValidationError) const
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

	if (!FMath::IsFinite(MinSpawnRadius) || !FMath::IsFinite(MaxSpawnRadius) || MinSpawnRadius < 0.0f || MaxSpawnRadius <= MinSpawnRadius)
	{
		SetValidationError(TEXT("Spawn radii must be finite and satisfy 0 <= MinSpawnRadius < MaxSpawnRadius."));

		return false;
	}

	if (!FMath::IsFinite(RockInterval) || RockInterval <= 0.0f)
	{
		SetValidationError(TEXT("RockInterval must be finite and greater than zero."));

		return false;
	}

	if (!FMath::IsFinite(ImpactDelay) || ImpactDelay <= 0.0f)
	{
		SetValidationError(TEXT("ImpactDelay must be finite and greater than zero."));

		return false;
	}

	if (!FMath::IsFinite(FallEffectLeadTime) || FallEffectLeadTime < 0.0f || FallEffectLeadTime > ImpactDelay)
	{
		SetValidationError(TEXT("FallEffectLeadTime must be finite and satisfy 0 <= FallEffectLeadTime <= ImpactDelay."));

		return false;
	}

	if (AttackShape.Type != ERSCombatShapeType::Sphere || AttackShape.InnerRadius != 0.0f || !AttackShape.IsDataValid())
	{
		SetValidationError(TEXT("AttackShape must be a valid Sphere with InnerRadius equal to zero."));

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

URSGameplayAbility_RandomFallingRocks::URSGameplayAbility_RandomFallingRocks()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	FallingRocksDefinition.AttackShape.Type = ERSCombatShapeType::Sphere;
	FallingRocksDefinition.AttackShape.Radius = 150.0f;
	FallingRocksDefinition.AttackShape.InnerRadius = 0.0f;
}

void URSGameplayAbility_RandomFallingRocks::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid() || !FallingRocksDefinition.IsDataValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	RandomStream.GenerateNewSeed();
	ScheduleNextRock();
}

void URSGameplayAbility_RandomFallingRocks::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (GenerationDelayTask)
	{
		GenerationDelayTask->EndTask();
		GenerationDelayTask = nullptr;
	}

	for (URSAbilityTask_PendingFallingRock* PendingRockTask : PendingRockTasks)
	{
		if (PendingRockTask)
		{
			PendingRockTask->EndTask();
		}
	}
	PendingRockTasks.Reset();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void URSGameplayAbility_RandomFallingRocks::ScheduleNextRock()
{
	if (!IsActive())
	{
		return;
	}

	GenerationDelayTask = UAbilityTask_WaitDelay::WaitDelay(this, FallingRocksDefinition.RockInterval);
	GenerationDelayTask->OnFinish.AddDynamic(this, &ThisClass::HandleRockIntervalFinished);
	GenerationDelayTask->ReadyForActivation();
}

void URSGameplayAbility_RandomFallingRocks::HandleRockIntervalFinished()
{
	GenerationDelayTask = nullptr;
	if (!IsActive())
	{
		return;
	}

	ReserveRock();
	ScheduleNextRock();
}

void URSGameplayAbility_RandomFallingRocks::ReserveRock()
{
	FVector SpawnCenter;
	if (!TryGetSpawnCenter(SpawnCenter))
	{
		return;
	}

	FVector2D ImpactOffset;
	if (!RSRandomPointSampling::TrySamplePointInAnnulus(FallingRocksDefinition.MinSpawnRadius, FallingRocksDefinition.MaxSpawnRadius, RandomStream, ImpactOffset))
	{
		return;
	}
	const FVector ImpactLocation = SpawnCenter + FVector(ImpactOffset, 0.0f);

	FRSTelegraphPresentation TelegraphPresentation;
	TelegraphPresentation.HoldDuration = FallingRocksDefinition.ImpactDelay;
	TelegraphPresentation.FillDuration = FallingRocksDefinition.ImpactDelay;
	TelegraphPresentation.Opacity = TelegraphOpacity;

	const FTransform ImpactTransform(ImpactLocation);
	const float FallEffectStartDelay = FallingRocksDefinition.ImpactDelay - FallingRocksDefinition.FallEffectLeadTime;
	URSAbilityTask_PendingFallingRock* PendingRockTask = URSAbilityTask_PendingFallingRock::CreatePendingFallingRock(this, ImpactTransform, FallingRocksDefinition.AttackShape, TelegraphPresentation, FallEffectStartDelay, FallingRocksDefinition.ImpactDelay, FallingRockNiagara);
	PendingRockTask->OnImpact.AddDynamic(this, &ThisClass::HandleRockImpact);
	PendingRockTasks.Add(PendingRockTask);
	PendingRockTask->ReadyForActivation();
}

void URSGameplayAbility_RandomFallingRocks::HandleRockImpact(FTransform ImpactTransform, URSAbilityTask_PendingFallingRock* PendingRockTask)
{
	PendingRockTasks.RemoveSingleSwap(PendingRockTask);

	AActor* AvatarActor = CurrentActorInfo ? CurrentActorInfo->AvatarActor.Get() : nullptr;
	if (!AvatarActor)
	{
		return;
	}

	TArray<AActor*> HitTargets;
	URSCombatFunctionLibrary::FindTargetsInShape(AvatarActor, TargetChannel, FallingRocksDefinition.AttackShape, ImpactTransform, HitTargets);

	const float DamageAmount = FallingRocksDefinition.Damage.GetValueAtLevel(GetAbilityLevel(CurrentSpecHandle, CurrentActorInfo));
	for (AActor* HitTarget : HitTargets)
	{
		ApplyDamageToTarget(HitTarget, DamageEffectClass, DamageAmount);
		URSCombatFunctionLibrary::SendHitReaction(AvatarActor, HitTarget, FallingRocksDefinition.Reaction);
	}

	if (ImpactNiagara)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, ImpactNiagara, ImpactTransform.GetLocation(), ImpactTransform.Rotator());
	}

	if (ImpactSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ImpactSound, ImpactTransform.GetLocation());
	}
}

bool URSGameplayAbility_RandomFallingRocks::TryGetSpawnCenter(FVector& OutSpawnCenter) const
{
	OutSpawnCenter = FVector::ZeroVector;

	const ACharacter* Character = CurrentActorInfo ? Cast<ACharacter>(CurrentActorInfo->AvatarActor.Get()) : nullptr;
	const UCapsuleComponent* CapsuleComp = Character ? Character->GetCapsuleComponent() : nullptr;
	if (!Character || !CapsuleComp)
	{
		return false;
	}

	OutSpawnCenter = Character->GetActorLocation() - FVector::UpVector * CapsuleComp->GetScaledCapsuleHalfHeight();

	return true;
}

#if WITH_EDITOR
EDataValidationResult URSGameplayAbility_RandomFallingRocks::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult ValidationResult = Super::IsDataValid(Context);

	FString ValidationError;
	if (!FallingRocksDefinition.IsDataValid(&ValidationError))
	{
		Context.AddError(FText::FromString(FString::Printf(TEXT("FallingRocksDefinition is invalid: %s"), *ValidationError)));
		ValidationResult = EDataValidationResult::Invalid;
	}

	if (!DamageEffectClass)
	{
		Context.AddError(FText::FromString(TEXT("DamageEffectClass is not configured.")));
		ValidationResult = EDataValidationResult::Invalid;
	}

	if (!FMath::IsFinite(TelegraphOpacity) || TelegraphOpacity < 0.0f || TelegraphOpacity > 1.0f)
	{
		Context.AddError(FText::FromString(TEXT("TelegraphOpacity must be finite and satisfy 0 <= TelegraphOpacity <= 1.")));
		ValidationResult = EDataValidationResult::Invalid;
	}

	return ValidationResult;
}
#endif
