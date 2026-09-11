#include "RSGameplayAbility_FlameCraterSpread.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Actors/RSBossFlameCrater.h"
#include "Combat/RSRandomPointSampling.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "RSGameplayTags.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace
{
	constexpr float FlameCraterSectorAngle = 45.0f;

	bool TryGetEffectiveRadii(const FRSFlameCraterSpreadDefinition& Definition, float& OutMinRadius, float& OutMaxRadius, float& OutLateralInset)
	{
		const float RadialInset = Definition.PlacementRadius + Definition.BoundaryMargin;
		OutMinRadius = Definition.MinSpawnRadius + RadialInset;
		OutMaxRadius = Definition.MaxSpawnRadius - RadialInset;
		OutLateralInset = Definition.PlacementRadius + Definition.BoundaryMargin + Definition.MinimumGap * 0.5f;

		return OutMinRadius > 0.0f && OutMaxRadius > OutMinRadius;
	}
}

bool FRSFlameCraterSpreadDefinition::IsDataValid(FString* OutValidationError) const
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

	if (!FMath::IsFinite(PlacementRadius) || !FMath::IsFinite(MinimumGap) || !FMath::IsFinite(BoundaryMargin) || PlacementRadius < 0.0f || MinimumGap < 0.0f || BoundaryMargin < 0.0f)
	{
		SetValidationError(TEXT("PlacementRadius, MinimumGap, and BoundaryMargin must be finite non-negative values."));

		return false;
	}

	float EffectiveMinRadius = 0.0f;
	float EffectiveMaxRadius = 0.0f;
	float LateralInset = 0.0f;
	if (!TryGetEffectiveRadii(*this, EffectiveMinRadius, EffectiveMaxRadius, LateralInset))
	{
		SetValidationError(TEXT("Placement radius and boundary margin leave no radial sampling range."));

		return false;
	}

	const float MaximumLateralInset = EffectiveMinRadius * FMath::Sin(FMath::DegreesToRadians(FlameCraterSectorAngle * 0.5f));
	if (LateralInset >= MaximumLateralInset)
	{
		SetValidationError(TEXT("Placement radius, gap, and boundary margin leave no angular sampling range in a 45 degree sector."));

		return false;
	}

	return true;
}

bool FRSFlameCraterSpreadDefinition::TryGenerateLandingLocations(const FVector& Center, const FVector& HorizontalForward, FRandomStream& InOutRandomStream, TArray<FVector>& OutLocations) const
{
	OutLocations.Reset();

	if (Center.ContainsNaN() || HorizontalForward.ContainsNaN() || !IsDataValid())
	{
		return false;
	}

	FVector Forward = HorizontalForward;
	Forward.Z = 0.0f;
	if (!Forward.Normalize())
	{
		return false;
	}

	float EffectiveMinRadius = 0.0f;
	float EffectiveMaxRadius = 0.0f;
	float LateralInset = 0.0f;
	if (!TryGetEffectiveRadii(*this, EffectiveMinRadius, EffectiveMaxRadius, LateralInset))
	{
		return false;
	}

	OutLocations.Reserve(FlameCraterCount);
	for (int32 SectorIndex = 0; SectorIndex < FlameCraterCount; ++SectorIndex)
	{
		float Radius = 0.0f;
		if (!RSRandomPointSampling::TrySampleRadiusInAnnulus(EffectiveMinRadius, EffectiveMaxRadius, InOutRandomStream, Radius))
		{
			OutLocations.Reset();

			return false;
		}

		const float AngularInset = FMath::RadiansToDegrees(FMath::Asin(LateralInset / Radius));
		const float SectorMinimumAngle = -90.0f + SectorIndex * FlameCraterSectorAngle + AngularInset;
		const float SectorMaximumAngle = -90.0f + (SectorIndex + 1) * FlameCraterSectorAngle - AngularInset;
		if (SectorMaximumAngle <= SectorMinimumAngle)
		{
			OutLocations.Reset();

			return false;
		}

		const float Angle = InOutRandomStream.FRandRange(SectorMinimumAngle, SectorMaximumAngle);
		const FVector Direction = Forward.RotateAngleAxis(Angle, FVector::UpVector);
		OutLocations.Add(Center + Direction * Radius);
	}

	return OutLocations.Num() == FlameCraterCount;
}

URSGameplayAbility_FlameCraterSpread::URSGameplayAbility_FlameCraterSpread()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(RSGameplayTags::Ability_Combat_FlameCraterSpread);
	SetAssetTags(AssetTags);

	ActivationBlockedTags.AddTag(RSGameplayTags::State_Action_Locked);
}

void URSGameplayAbility_FlameCraterSpread::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	ActiveLandingLocations.Reset();
	bDropFinished = false;
	bRoarMontageFinished = !RoarMontage;

	if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid() || !FlameCraterClass || !SpreadDefinition.IsDataValid() || !CaptureLandingLocations(ActorInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	if (RoarMontage)
	{
		UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, RoarMontage);
		MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleRoarMontageFinished);
		MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::HandleRoarMontageFinished);
		MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleRoarMontageCancelled);
		MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleRoarMontageCancelled);
		MontageTask->ReadyForActivation();
	}

	if (DropDelay <= 0.0f)
	{
		HandleDropDelayFinished();

		return;
	}

	UAbilityTask_WaitDelay* DropDelayTask = UAbilityTask_WaitDelay::WaitDelay(this, DropDelay);
	DropDelayTask->OnFinish.AddDynamic(this, &ThisClass::HandleDropDelayFinished);
	DropDelayTask->ReadyForActivation();
}

void URSGameplayAbility_FlameCraterSpread::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	EndAnimationGameplayStatesForMontage(ActorInfo, RoarMontage);
	ActiveLandingLocations.Reset();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

#if WITH_EDITOR
EDataValidationResult URSGameplayAbility_FlameCraterSpread::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult ValidationResult = Super::IsDataValid(Context);

	FString ValidationError;
	if (!SpreadDefinition.IsDataValid(&ValidationError))
	{
		Context.AddError(FText::FromString(FString::Printf(TEXT("SpreadDefinition is invalid: %s"), *ValidationError)));
		ValidationResult = EDataValidationResult::Invalid;
	}

	if (!FlameCraterClass)
	{
		Context.AddError(FText::FromString(TEXT("FlameCraterClass is not configured.")));
		ValidationResult = EDataValidationResult::Invalid;
	}

	if (!FMath::IsFinite(DropDelay) || DropDelay < 0.0f)
	{
		Context.AddError(FText::FromString(TEXT("DropDelay must be a finite non-negative value.")));
		ValidationResult = EDataValidationResult::Invalid;
	}

	return ValidationResult;
}
#endif

bool URSGameplayAbility_FlameCraterSpread::CaptureLandingLocations(const FGameplayAbilityActorInfo* ActorInfo)
{
	const ACharacter* BossCharacter = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	const ACharacter* PlayerCharacter = GetWorld() ? UGameplayStatics::GetPlayerCharacter(GetWorld(), 0) : nullptr;
	if (!BossCharacter || !PlayerCharacter)
	{
		return false;
	}

	const FVector Center = BossCharacter->GetActorLocation() - FVector::UpVector * BossCharacter->GetSimpleCollisionHalfHeight();
	FVector PlayerDirection = PlayerCharacter->GetActorLocation() - Center;
	PlayerDirection.Z = 0.0f;
	if (!PlayerDirection.Normalize())
	{
		PlayerDirection = BossCharacter->GetActorForwardVector();
		PlayerDirection.Z = 0.0f;
	}

	RandomStream.GenerateNewSeed();

	return SpreadDefinition.TryGenerateLandingLocations(Center, PlayerDirection, RandomStream, ActiveLandingLocations);
}

bool URSGameplayAbility_FlameCraterSpread::SpawnFlameCraters()
{
	UWorld* World = GetWorld();
	AActor* AvatarActor = CurrentActorInfo ? CurrentActorInfo->AvatarActor.Get() : nullptr;
	if (!World || !AvatarActor || ActiveLandingLocations.Num() != FRSFlameCraterSpreadDefinition::FlameCraterCount)
	{
		return false;
	}

	TArray<ARSBossFlameCrater*> SpawnedFlameCraters;
	SpawnedFlameCraters.Reserve(ActiveLandingLocations.Num());
	for (const FVector& LandingLocation : ActiveLandingLocations)
	{
		const FTransform SpawnTransform(LandingLocation);
		ARSBossFlameCrater* FlameCrater = World->SpawnActorDeferred<ARSBossFlameCrater>(FlameCraterClass, SpawnTransform, AvatarActor, Cast<APawn>(AvatarActor), ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!FlameCrater)
		{
			for (ARSBossFlameCrater* SpawnedFlameCrater : SpawnedFlameCraters)
			{
				if (SpawnedFlameCrater)
				{
					SpawnedFlameCrater->RequestCleanup();
				}
			}

			return false;
		}

		UGameplayStatics::FinishSpawningActor(FlameCrater, SpawnTransform);
		SpawnedFlameCraters.Add(FlameCrater);
	}

	return true;
}

void URSGameplayAbility_FlameCraterSpread::HandleDropDelayFinished()
{
	if (!IsActive() || bDropFinished)
	{
		return;
	}

	bDropFinished = SpawnFlameCraters();
	if (!bDropFinished)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	TryFinishAbility();
}

void URSGameplayAbility_FlameCraterSpread::HandleRoarMontageFinished()
{
	if (!IsActive())
	{
		return;
	}

	bRoarMontageFinished = true;
	TryFinishAbility();
}

void URSGameplayAbility_FlameCraterSpread::HandleRoarMontageCancelled()
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

void URSGameplayAbility_FlameCraterSpread::TryFinishAbility()
{
	if (IsActive() && bDropFinished && bRoarMontageFinished)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}
