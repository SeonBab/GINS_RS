#include "RSGameplayAbility_FireballSpread.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Actors/RSBossFireball.h"
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
	/** 대상 180도를 화염구 개수만큼 균등 분할한 한 구역의 폭입니다 */
	float GetSectorAngle(const FRSFireballSpreadDefinition& Definition)
	{
		return 180.0f / Definition.FireballCount;
	}

	bool TryGetEffectiveRadii(const FRSFireballSpreadDefinition& Definition, float& OutMinRadius, float& OutMaxRadius, float& OutLateralInset)
	{
		const float RadialInset = Definition.PlacementRadius + Definition.BoundaryMargin;
		OutMinRadius = Definition.MinSpawnRadius + RadialInset;
		OutMaxRadius = Definition.MaxSpawnRadius - RadialInset;
		OutLateralInset = Definition.PlacementRadius + Definition.BoundaryMargin + Definition.MinimumGap * 0.5f;

		return OutMinRadius > 0.0f && OutMaxRadius > OutMinRadius;
	}
}

bool FRSFireballSpreadDefinition::IsDataValid(FString* OutValidationError) const
{
	if (OutValidationError)
	{
		OutValidationError->Reset();
	}

	auto SetValidationError = [OutValidationError](const FString& ErrorMessage)
		{
			if (OutValidationError)
			{
				*OutValidationError = ErrorMessage;
			}
		};

	if (FireballCount < 1)
	{
		SetValidationError(TEXT("FireballCount must be at least 1."));

		return false;
	}

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

	const float MaximumLateralInset = EffectiveMinRadius * FMath::Sin(FMath::DegreesToRadians(FireballSectorAngle * 0.5f));
	if (LateralInset >= MaximumLateralInset)
	{
		SetValidationError(TEXT("Placement radius, gap, and boundary margin leave no angular sampling range in a 45 degree sector."));

		return false;
	}

	return true;
}

bool FRSFireballSpreadDefinition::TryGenerateLandingLocations(const FVector& Center, const FVector& HorizontalForward, FRandomStream& InOutRandomStream, TArray<FVector>& OutLocations) const
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

	OutLocations.Reserve(FireballCount);
	for (int32 SectorIndex = 0; SectorIndex < FireballCount; ++SectorIndex)
	{
		float Radius = 0.0f;
		if (!RSRandomPointSampling::TrySampleRadiusInAnnulus(EffectiveMinRadius, EffectiveMaxRadius, InOutRandomStream, Radius))
		{
			OutLocations.Reset();

			return false;
		}

		const float AngularInset = FMath::RadiansToDegrees(FMath::Asin(LateralInset / Radius));
		const float SectorMinimumAngle = -90.0f + SectorIndex * FireballSectorAngle + AngularInset;
		const float SectorMaximumAngle = -90.0f + (SectorIndex + 1) * FireballSectorAngle - AngularInset;
		if (SectorMaximumAngle <= SectorMinimumAngle)
		{
			OutLocations.Reset();

			return false;
		}

		const float Angle = InOutRandomStream.FRandRange(SectorMinimumAngle, SectorMaximumAngle);
		const FVector Direction = Forward.RotateAngleAxis(Angle, FVector::UpVector);
		OutLocations.Add(Center + Direction * Radius);
	}

	return OutLocations.Num() == FireballCount;
}

URSGameplayAbility_FireballSpread::URSGameplayAbility_FireballSpread()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(RSGameplayTags::Ability_Combat_FireballSpread);
	SetAssetTags(AssetTags);

	ActivationBlockedTags.AddTag(RSGameplayTags::State_Action_Locked);
}

void URSGameplayAbility_FireballSpread::BeginPatternTimeline()
{
	ActiveLandingLocations.Reset();
	bDropFinished = false;
	bRoarMontageFinished = !RoarMontage;

	if (!CurrentActorInfo || !CurrentActorInfo->AbilitySystemComponent.IsValid() || !FireballClass || !SpreadDefinition.IsDataValid() || !CaptureLandingLocations(CurrentActorInfo))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	if (!CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

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

void URSGameplayAbility_FireballSpread::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	EndAnimationGameplayStatesForMontage(ActorInfo, RoarMontage);
	ActiveLandingLocations.Reset();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

#if WITH_EDITOR
EDataValidationResult URSGameplayAbility_FireballSpread::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult ValidationResult = Super::IsDataValid(Context);

	FString ValidationError;
	if (!SpreadDefinition.IsDataValid(&ValidationError))
	{
		Context.AddError(FText::FromString(FString::Printf(TEXT("SpreadDefinition is invalid: %s"), *ValidationError)));
		ValidationResult = EDataValidationResult::Invalid;
	}

	if (!FireballDefinition.IsDataValid(&ValidationError))
	{
		Context.AddError(FText::FromString(FString::Printf(TEXT("FireballDefinition is invalid: %s"), *ValidationError)));
		ValidationResult = EDataValidationResult::Invalid;
	}

	if (!FireballClass)
	{
		Context.AddError(FText::FromString(TEXT("FireballClass is not configured.")));
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

bool URSGameplayAbility_FireballSpread::CaptureLandingLocations(const FGameplayAbilityActorInfo* ActorInfo)
{
	const ACharacter* BossCharacter = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	const ACharacter* PlayerCharacter = GetWorld() ? UGameplayStatics::GetPlayerCharacter(GetWorld(), 0) : nullptr;
	if (!BossCharacter || !PlayerCharacter)
	{
		return false;
	}

	FVector Center = FVector::ZeroVector;
	if (!URSCombatFunctionLibrary::TryGetActorGroundLocation(BossCharacter, Center))
	{
		return false;
	}

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

bool URSGameplayAbility_FireballSpread::SpawnFireballs()
{
	UWorld* World = GetWorld();
	AActor* AvatarActor = CurrentActorInfo ? CurrentActorInfo->AvatarActor.Get() : nullptr;
	if (!World || !AvatarActor || ActiveLandingLocations.Num() != SpreadDefinition.FireballCount)
	{
		return false;
	}

	TArray<ARSBossFireball*> SpawnedFireballs;
	SpawnedFireballs.Reserve(ActiveLandingLocations.Num());
	for (const FVector& LandingLocation : ActiveLandingLocations)
	{
		const FTransform SpawnTransform(LandingLocation);
		ARSBossFireball* Fireball = World->SpawnActorDeferred<ARSBossFireball>(FireballClass, SpawnTransform, AvatarActor, Cast<APawn>(AvatarActor), ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Fireball)
		{
			for (ARSBossFireball* SpawnedFireball : SpawnedFireballs)
			{
				if (SpawnedFireball)
				{
					SpawnedFireball->RequestCleanup();
				}
			}

			return false;
		}

		Fireball->Initialize(FireballDefinition, MakePatternHitSpec());
		UGameplayStatics::FinishSpawningActor(Fireball, SpawnTransform);
		SpawnedFireballs.Add(Fireball);
	}

	return true;
}

void URSGameplayAbility_FireballSpread::HandleDropDelayFinished()
{
	if (!IsActive() || bDropFinished)
	{
		return;
	}

	bDropFinished = SpawnFireballs();
	if (!bDropFinished)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	// 던지는 순간의 연출을 보스 발밑에서 한 번 재생합니다
	// 화염구 자체는 Actor가 자기 Niagara로 보여 주므로 이 정의의 Niagara는 보통 비워 두며, 빈 항목은 그대로 건너뜁니다
	FVector PresentationLocation = FVector::ZeroVector;
	if (URSCombatFunctionLibrary::TryGetActorGroundLocation(GetAvatarActorFromActorInfo(), PresentationLocation))
	{
		PlayPatternPresentation(FTransform(PresentationLocation));
	}

	TryFinishAbility();
}

void URSGameplayAbility_FireballSpread::HandleRoarMontageFinished()
{
	if (!IsActive())
	{
		return;
	}

	bRoarMontageFinished = true;
	TryFinishAbility();
}

void URSGameplayAbility_FireballSpread::HandleRoarMontageCancelled()
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

void URSGameplayAbility_FireballSpread::TryFinishAbility()
{
	if (IsActive() && bDropFinished && bRoarMontageFinished)
	{
		FinishPatternWhenMontageEnds();
	}
}
