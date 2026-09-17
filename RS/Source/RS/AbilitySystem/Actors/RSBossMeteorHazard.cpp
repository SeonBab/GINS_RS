#include "RSBossMeteorHazard.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/SceneComponent.h"
#include "Components/RSAttackTelegraphComponent.h"
#include "Combat/RSCombatFunctionLibrary.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "NiagaraComponent.h"
#include "RSBossPersistentObjectLifetimeComponent.h"
#include "RSGameplayTags.h"
#include "RSPlayerCharacter.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace
{
	constexpr float MeteorTargetLockProgress = 0.75f;
	constexpr float MeteorImpactEffectHoldDuration = 0.3f;
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

	if (!FMath::IsFinite(HazardTelegraphAlpha) || HazardTelegraphAlpha <= 0.0f || HazardTelegraphAlpha > 1.0f)
	{
		SetValidationError(TEXT("HazardTelegraphAlpha must be finite and satisfy 0 < HazardTelegraphAlpha <= 1."));

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

	TelegraphComp = CreateDefaultSubobject<URSAttackTelegraphComponent>(TEXT("AttackTelegraphComponent"));
	PersistentObjectLifetimeComp = CreateDefaultSubobject<URSBossPersistentObjectLifetimeComponent>(TEXT("PersistentObjectLifetimeComponent"));
}

void ARSBossMeteorHazard::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (MeteorHazardState == ERSBossMeteorHazardState::Hazard)
	{
		KeepFallingEffectActive();

		return;
	}

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
	if (!TelegraphComp || !TelegraphComp->SetShapeTransform(TelegraphHandle, FTransform(DisplayLocation)) || !TelegraphComp->SetExternalFill(TelegraphHandle, Fill))
	{
		RequestCleanup();

		return;
	}

	if (bFallingEffectStarted)
	{
		UpdateFallingEffectLocation(DisplayLocation);
		KeepFallingEffectActive();
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

	if (!MeteorHazardDefinition.IsDataValid() || !HitSpec.IsValid() || !TelegraphComp || !UpdateTargetLocation())
	{
		RequestCleanup();

		return;
	}

	FRSCombatShape TelegraphShape;
	TelegraphShape.Type = ERSCombatShapeType::AnnularSector;
	TelegraphShape.OuterRadius = MeteorHazardDefinition.HazardRadius;
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

	if (!FallingNiagaraComp || !FallingNiagaraComp->GetAsset())
	{
		Context.AddError(FText::FromString(TEXT("FallingNiagaraComponent has no Niagara System asset.")));
		ValidationResult = EDataValidationResult::Invalid;
	}

	if (!HazardNiagaraComp || !HazardNiagaraComp->GetAsset())
	{
		Context.AddError(FText::FromString(TEXT("HazardNiagaraComponent has no Niagara System asset.")));
		ValidationResult = EDataValidationResult::Invalid;
	}

	if (!TelegraphComp || !TelegraphComp->HasDecalMaterial())
	{
		Context.AddError(FText::FromString(TEXT("AttackTelegraphComponent has no DecalMaterial.")));
		ValidationResult = EDataValidationResult::Invalid;
	}

	return ValidationResult;
}
#endif

void ARSBossMeteorHazard::Initialize(AActor* InTargetActor, const FRSBossMeteorHazardDefinition& InMeteorHazardDefinition, const FRSBossPatternHitSpec& InHitSpec)
{
	TargetActor = InTargetActor;
	MeteorHazardDefinition = InMeteorHazardDefinition;
	HitSpec = InHitSpec;
}

#if WITH_DEV_AUTOMATION_TESTS
bool ARSBossMeteorHazard::IsDamageTimerActiveForTest() const
{
	UWorld* World = GetWorld();

	return World && World->GetTimerManager().IsTimerActive(HazardDamageTimerHandle);
}

bool ARSBossMeteorHazard::IsFallingEffectFinishTimerActiveForTest() const
{
	UWorld* World = GetWorld();

	return World && World->GetTimerManager().IsTimerActive(FallingEffectFinishTimerHandle);
}

float ARSBossMeteorHazard::GetFallingEffectFinishTimerRemainingForTest() const
{
	UWorld* World = GetWorld();

	return World ? World->GetTimerManager().GetTimerRemaining(FallingEffectFinishTimerHandle) : -1.0f;
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

	return URSCombatFunctionLibrary::TryGetActorGroundLocation(ActiveTargetActor, CurrentTargetLocation);
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

	if (FallingNiagaraComp)
	{
		FallingNiagaraComp->Activate(true);
	}
}

void ARSBossMeteorHazard::UpdateFallingEffectLocation(const FVector& TargetLocation)
{
	if (FallingNiagaraComp)
	{
		const float FallingProgress = MeteorHazardDefinition.FallEffectLeadTime > UE_SMALL_NUMBER
			? FMath::Clamp((TelegraphElapsedTime - MeteorHazardDefinition.GetFallEffectStartTime()) / MeteorHazardDefinition.FallEffectLeadTime, 0.0f, 1.0f)
			: 1.0f;
		const FVector StartOffset = FVector::UpVector * MeteorHazardDefinition.FallEffectHeight;
		FallingNiagaraComp->SetWorldLocation(TargetLocation + StartOffset * (1.0f - FallingProgress));
	}
}

void ARSBossMeteorHazard::FinishFallingEffect()
{
	if (FallingNiagaraComp)
	{
		FallingNiagaraComp->DeactivateImmediate();
	}

	SetActorTickEnabled(false);
}

void ARSBossMeteorHazard::KeepFallingEffectActive()
{
	if (FallingNiagaraComp && !FallingNiagaraComp->IsActive())
	{
		FallingNiagaraComp->Activate(true);
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
	SetActorLocation(LockedImpactLocation);
	SwitchTelegraphToHazard();

	if (HazardNiagaraComp)
	{
		HazardNiagaraComp->SetWorldLocation(LockedImpactLocation);
		HazardNiagaraComp->Activate(true);
	}

	if (FallingNiagaraComp)
	{
		FallingNiagaraComp->Activate(true);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(FallingEffectFinishTimerHandle, this, &ThisClass::FinishFallingEffect, MeteorImpactEffectHoldDuration, false);
		World->GetTimerManager().SetTimer(HazardDamageTimerHandle, this, &ThisClass::ApplyHazardDamage, MeteorHazardDefinition.DamageInterval, true, MeteorHazardDefinition.DamageInterval);
	}
	else
	{
		FinishFallingEffect();
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
	HazardShape.Type = ERSCombatShapeType::AnnularSector;
	HazardShape.OuterRadius = MeteorHazardDefinition.HazardRadius;
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
		DamagePayload.EventMagnitude = HitSpec.DamageAmount;
		DamagePayload.OptionalObject = HitSpec.DamageEffectClass->GetDefaultObject<UGameplayEffect>();
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwnerActor, DamagePayload.EventTag, DamagePayload);
		URSCombatFunctionLibrary::SendHitReaction(this, HitTarget, HitSpec.Reaction);
	}
}

void ARSBossMeteorHazard::SwitchTelegraphToHazard()
{
	if (!TelegraphComp || TelegraphHandle == INDEX_NONE)
	{
		return;
	}

	// 예고에 쓰던 표시를 그대로 이어 쓰므로 장판이 시작될 때 범위가 비는 프레임이 없습니다
	// 판정이 반복되는 동안에는 채움을 움직이지 않고 불투명도만 낮춰 예고와 구분합니다
	TelegraphComp->SetShapeTransform(TelegraphHandle, FTransform(LockedImpactLocation));
	TelegraphComp->SetExternalFill(TelegraphHandle, 1.0f);
	TelegraphComp->SetAlpha(TelegraphHandle, MeteorHazardDefinition.HazardTelegraphAlpha);
}

void ARSBossMeteorHazard::HideTelegraph()
{
	if (TelegraphComp)
	{
		TelegraphComp->HideShape(TelegraphHandle);
	}

	TelegraphHandle = INDEX_NONE;
}

void ARSBossMeteorHazard::ClearTimers()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FallingEffectFinishTimerHandle);
		World->GetTimerManager().ClearTimer(HazardDamageTimerHandle);
	}

	FallingEffectFinishTimerHandle.Invalidate();
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
