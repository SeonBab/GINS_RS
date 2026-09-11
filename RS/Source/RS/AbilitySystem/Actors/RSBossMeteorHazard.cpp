#include "RSBossMeteorHazard.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/RSAttackTelegraphComponent.h"
#include "Combat/RSCombatFunctionLibrary.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "RSBossPersistentObjectLifetimeComponent.h"
#include "RSGameplayTags.h"
#include "RSPlayerCharacter.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace
{
	constexpr float MeteorTargetLockProgress = 0.75f;
}

bool FRSBossMeteorHazardDefinition::IsDataValid(FString* OutValidationError) const
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

	if (!FMath::IsFinite(TelegraphDuration) || TelegraphDuration <= 0.0f)
	{
		SetValidationError(TEXT("TelegraphDuration must be a finite value greater than zero."));

		return false;
	}

	if (!FMath::IsFinite(FallEffectLeadTime) || FallEffectLeadTime < 0.0f || FallEffectLeadTime > TelegraphDuration)
	{
		SetValidationError(TEXT("FallEffectLeadTime must be finite and satisfy 0 <= FallEffectLeadTime <= TelegraphDuration."));

		return false;
	}

	if (!FMath::IsFinite(HazardRadius) || HazardRadius <= 0.0f || !FMath::IsFinite(DamageInterval) || DamageInterval <= 0.0f)
	{
		SetValidationError(TEXT("HazardRadius and DamageInterval must be finite values greater than zero."));

		return false;
	}

	if (SpecialPatternCleanupOffset <= 0)
	{
		SetValidationError(TEXT("SpecialPatternCleanupOffset must be greater than zero."));

		return false;
	}

	return true;
}

float FRSBossMeteorHazardDefinition::GetFallEffectStartTime() const
{
	return FMath::Max(TelegraphDuration - FallEffectLeadTime, 0.0f);
}

ARSBossMeteorHazard::ARSBossMeteorHazard()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	FallingNiagaraComp = CreateDefaultSubobject<UNiagaraComponent>(TEXT("FallingNiagaraComponent"));
	FallingNiagaraComp->SetupAttachment(SceneRoot);
	FallingNiagaraComp->SetUsingAbsoluteLocation(true);
	FallingNiagaraComp->SetAutoActivate(false);

	HazardNiagaraComp = CreateDefaultSubobject<UNiagaraComponent>(TEXT("HazardNiagaraComponent"));
	HazardNiagaraComp->SetupAttachment(SceneRoot);
	HazardNiagaraComp->SetUsingAbsoluteLocation(true);
	HazardNiagaraComp->SetAutoActivate(false);

	PersistentObjectLifetimeComp = CreateDefaultSubobject<URSBossPersistentObjectLifetimeComponent>(TEXT("PersistentObjectLifetimeComponent"));
}

void ARSBossMeteorHazard::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (MeteorHazardState != ERSBossMeteorHazardState::Tracking && MeteorHazardState != ERSBossMeteorHazardState::Locked)
	{
		SetActorTickEnabled(false);

		return;
	}

	if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds < 0.0f)
	{
		return;
	}

	if (MeteorHazardState == ERSBossMeteorHazardState::Tracking && !UpdateTargetLocation())
	{
		RequestCleanup();

		return;
	}

	TelegraphElapsedTime = FMath::Min(TelegraphElapsedTime + DeltaSeconds, MeteorHazardDefinition.TelegraphDuration);
	const float Fill = TelegraphElapsedTime / MeteorHazardDefinition.TelegraphDuration;

	if (!bFallingEffectStarted && TelegraphElapsedTime >= MeteorHazardDefinition.GetFallEffectStartTime())
	{
		StartFallingEffect();
	}

	if (MeteorHazardState == ERSBossMeteorHazardState::Tracking && Fill >= MeteorTargetLockProgress)
	{
		LockImpactLocation();
	}

	const FVector DisplayLocation = MeteorHazardState == ERSBossMeteorHazardState::Tracking ? CurrentTargetLocation : LockedImpactLocation;
	URSAttackTelegraphComponent* ActiveTelegraphComp = TelegraphComp.Get();
	if (!ActiveTelegraphComp || !ActiveTelegraphComp->SetShapeTransform(TelegraphHandle, FTransform(DisplayLocation)) || !ActiveTelegraphComp->SetExternalFill(TelegraphHandle, Fill))
	{
		RequestCleanup();

		return;
	}

	if (bFallingEffectStarted)
	{
		UpdateFallingEffectLocation(DisplayLocation);
	}

	if (Fill >= 1.0f)
	{
		BeginHazard();
	}
}

void ARSBossMeteorHazard::BeginPlay()
{
	Super::BeginPlay();

	if (PersistentObjectLifetimeComp)
	{
		PersistentObjectLifetimeComp->OnCleanupRequired().AddUObject(this, &ThisClass::RequestCleanup);
		PersistentObjectLifetimeComp->StartObserving(GetOwner(), MeteorHazardDefinition.SpecialPatternCleanupOffset);
	}

	if (FallingNiagaraComp)
	{
		FallingNiagaraComp->SetAsset(FallingNiagaraSystem);
	}

	if (HazardNiagaraComp)
	{
		HazardNiagaraComp->SetAsset(HazardNiagaraSystem);
	}

	AActor* OwnerActor = GetOwner();
	TelegraphComp = OwnerActor ? OwnerActor->FindComponentByClass<URSAttackTelegraphComponent>() : nullptr;
	if (!MeteorHazardDefinition.IsDataValid() || !TelegraphComp.IsValid() || !UpdateTargetLocation())
	{
		RequestCleanup();

		return;
	}

	FRSCombatShape TelegraphShape;
	TelegraphShape.Type = ERSCombatShapeType::Sphere;
	TelegraphShape.Radius = MeteorHazardDefinition.HazardRadius;
	TelegraphShape.InnerRadius = 0.0f;
	TelegraphHandle = TelegraphComp->ShowShapeWithExternalFill(TelegraphShape, FTransform(CurrentTargetLocation));
	if (TelegraphHandle == INDEX_NONE)
	{
		RequestCleanup();

		return;
	}

	if (MeteorHazardDefinition.GetFallEffectStartTime() <= 0.0f)
	{
		StartFallingEffect();
	}

	SetActorTickEnabled(true);
}

void ARSBossMeteorHazard::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetActorTickEnabled(false);
	HideTelegraph();
	ClearTimers();
	if (PersistentObjectLifetimeComp)
	{
		PersistentObjectLifetimeComp->OnCleanupRequired().RemoveAll(this);
		PersistentObjectLifetimeComp->StopObserving();
	}

	if (FallingNiagaraComp)
	{
		FallingNiagaraComp->DeactivateImmediate();
	}

	if (HazardNiagaraComp)
	{
		HazardNiagaraComp->DeactivateImmediate();
	}

	Super::EndPlay(EndPlayReason);
}

#if WITH_EDITOR
EDataValidationResult ARSBossMeteorHazard::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult ValidationResult = Super::IsDataValid(Context);

	FString ValidationError;
	if (!MeteorHazardDefinition.IsDataValid(&ValidationError))
	{
		Context.AddError(FText::FromString(FString::Printf(TEXT("MeteorHazardDefinition is invalid: %s"), *ValidationError)));
		ValidationResult = EDataValidationResult::Invalid;
	}

	if (!FMath::IsFinite(HazardNiagaraBaseRadius) || HazardNiagaraBaseRadius <= 0.0f)
	{
		Context.AddError(FText::FromString(TEXT("HazardNiagaraBaseRadius must be a finite value greater than zero.")));
		ValidationResult = EDataValidationResult::Invalid;
	}

	return ValidationResult;
}
#endif

void ARSBossMeteorHazard::Initialize(AActor* InTargetActor)
{
	TargetActor = InTargetActor;
}

#if WITH_DEV_AUTOMATION_TESTS
bool ARSBossMeteorHazard::IsDamageTimerActiveForTest() const
{
	UWorld* World = GetWorld();

	return World && World->GetTimerManager().IsTimerActive(HazardDamageTimerHandle);
}

float ARSBossMeteorHazard::GetDamageTimerRemainingForTest() const
{
	UWorld* World = GetWorld();

	return World ? World->GetTimerManager().GetTimerRemaining(HazardDamageTimerHandle) : -1.0f;
}

void ARSBossMeteorHazard::ApplyHazardDamageForTest()
{
	ApplyHazardDamage();
}
#endif

void ARSBossMeteorHazard::RequestCleanup()
{
	if (MeteorHazardState == ERSBossMeteorHazardState::Cleanup)
	{
		return;
	}

	MeteorHazardState = ERSBossMeteorHazardState::Cleanup;
	SetActorTickEnabled(false);
	HideTelegraph();
	ClearTimers();

	if (FallingNiagaraComp)
	{
		FallingNiagaraComp->DeactivateImmediate();
	}

	if (HazardNiagaraComp)
	{
		HazardNiagaraComp->DeactivateImmediate();
	}

	BroadcastPreparationFinished(ERSBossMeteorHazardPreparationResult::CleanedUp);
	Destroy();
}

bool ARSBossMeteorHazard::UpdateTargetLocation()
{
	AActor* ActiveTargetActor = TargetActor.Get();
	if (!IsValid(ActiveTargetActor))
	{
		return false;
	}

	const UAbilitySystemComponent* TargetAbilitySystemComp = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(ActiveTargetActor);
	if (TargetAbilitySystemComp && TargetAbilitySystemComp->HasMatchingGameplayTag(RSGameplayTags::State_Dead))
	{
		return false;
	}

	const ACharacter* TargetCharacter = Cast<ACharacter>(ActiveTargetActor);
	const UCapsuleComponent* TargetCapsuleComp = TargetCharacter ? TargetCharacter->GetCapsuleComponent() : nullptr;
	CurrentTargetLocation = TargetCapsuleComp
		? TargetCapsuleComp->GetComponentLocation() - FVector::UpVector * TargetCapsuleComp->GetScaledCapsuleHalfHeight()
		: ActiveTargetActor->GetActorLocation();

	return !CurrentTargetLocation.ContainsNaN();
}

void ARSBossMeteorHazard::StartFallingEffect()
{
	if (bFallingEffectStarted)
	{
		return;
	}

	bFallingEffectStarted = true;
	const FVector TargetLocation = MeteorHazardState == ERSBossMeteorHazardState::Tracking ? CurrentTargetLocation : LockedImpactLocation;
	UpdateFallingEffectLocation(TargetLocation);

	if (FallingNiagaraSystem && FallingNiagaraComp)
	{
		FallingNiagaraComp->Activate(true);
	}
}

void ARSBossMeteorHazard::UpdateFallingEffectLocation(const FVector& TargetLocation)
{
	if (FallingNiagaraComp)
	{
		FallingNiagaraComp->SetWorldLocation(TargetLocation);
	}
}

void ARSBossMeteorHazard::LockImpactLocation()
{
	if (MeteorHazardState != ERSBossMeteorHazardState::Tracking)
	{
		return;
	}

	LockedImpactLocation = CurrentTargetLocation;
	MeteorHazardState = ERSBossMeteorHazardState::Locked;
}

void ARSBossMeteorHazard::BeginHazard()
{
	if (MeteorHazardState == ERSBossMeteorHazardState::Hazard || MeteorHazardState == ERSBossMeteorHazardState::Cleanup)
	{
		return;
	}

	if (MeteorHazardState == ERSBossMeteorHazardState::Tracking)
	{
		LockImpactLocation();
	}

	MeteorHazardState = ERSBossMeteorHazardState::Hazard;
	SetActorTickEnabled(false);
	SetActorLocation(LockedImpactLocation);
	HideTelegraph();

	if (FallingNiagaraComp)
	{
		FallingNiagaraComp->DeactivateImmediate();
	}

	if (HazardNiagaraComp)
	{
		HazardNiagaraComp->SetWorldLocation(LockedImpactLocation);
		const float HorizontalScale = MeteorHazardDefinition.HazardRadius / HazardNiagaraBaseRadius;
		HazardNiagaraComp->SetRelativeScale3D(FVector(HorizontalScale, HorizontalScale, 1.0f));
		if (HazardNiagaraSystem)
		{
			HazardNiagaraComp->Activate(true);
		}
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(HazardDamageTimerHandle, this, &ThisClass::ApplyHazardDamage, MeteorHazardDefinition.DamageInterval, true, MeteorHazardDefinition.DamageInterval);
	}

	BroadcastPreparationFinished(ERSBossMeteorHazardPreparationResult::HazardActivated);
}

void ARSBossMeteorHazard::ApplyHazardDamage()
{
	if (MeteorHazardState != ERSBossMeteorHazardState::Hazard)
	{
		return;
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	FRSCombatShape HazardShape;
	HazardShape.Type = ERSCombatShapeType::Sphere;
	HazardShape.Radius = MeteorHazardDefinition.HazardRadius;
	HazardShape.InnerRadius = 0.0f;

	TArray<AActor*> HitTargets;
	URSCombatFunctionLibrary::FindTargetsInShape(this, PlayerTargetChannel, HazardShape, FTransform(LockedImpactLocation), HitTargets);
	for (AActor* HitTarget : HitTargets)
	{
		if (!Cast<ARSPlayerCharacter>(HitTarget))
		{
			continue;
		}

		FGameplayEventData DamagePayload;
		DamagePayload.EventTag = RSGameplayTags::GameplayEvent_Combat_MeteorHazardDamage;
		DamagePayload.Instigator = this;
		DamagePayload.Target = HitTarget;
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwnerActor, DamagePayload.EventTag, DamagePayload);
	}
}

void ARSBossMeteorHazard::HideTelegraph()
{
	if (URSAttackTelegraphComponent* ActiveTelegraphComp = TelegraphComp.Get())
	{
		ActiveTelegraphComp->HideShape(TelegraphHandle);
	}

	TelegraphHandle = INDEX_NONE;
	TelegraphComp.Reset();
}

void ARSBossMeteorHazard::ClearTimers()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HazardDamageTimerHandle);
	}

	HazardDamageTimerHandle.Invalidate();
}

void ARSBossMeteorHazard::BroadcastPreparationFinished(ERSBossMeteorHazardPreparationResult PreparationResult)
{
	if (bPreparationFinishedBroadcast)
	{
		return;
	}

	bPreparationFinishedBroadcast = true;
	PreparationFinishedEvent.Broadcast(PreparationResult);
}
