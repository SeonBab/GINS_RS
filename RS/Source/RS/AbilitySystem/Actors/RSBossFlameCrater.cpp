#include "RSBossFlameCrater.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Combat/RSCombatFunctionLibrary.h"
#include "Curves/CurveFloat.h"
#include "Engine/World.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "RSAbilitySystemComponent.h"
#include "RSBossPersistentObjectLifetimeComponent.h"
#include "RSFlameCraterChargeWidget.h"
#include "RSGameplayTags.h"
#include "RSHealthComponent.h"
#include "RSHealthSet.h"
#include "RSPlayerCharacter.h"
#include "TimerManager.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

bool FRSBossFlameCraterDefinition::IsDataValid(FString* OutValidationError) const
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

	if (!FMath::IsFinite(FallHeight) || FallHeight < 0.0f || !FMath::IsFinite(FallDuration) || FallDuration <= 0.0f)
	{
		SetValidationError(TEXT("FallHeight must be non-negative and FallDuration must be greater than zero."));

		return false;
	}

	if (!FMath::IsFinite(ChargeDuration) || ChargeDuration <= 0.0f || RequiredHitCount <= 0)
	{
		SetValidationError(TEXT("ChargeDuration and RequiredHitCount must be greater than zero."));

		return false;
	}

	if (!FMath::IsFinite(FireFieldRadius) || FireFieldRadius <= 0.0f || !FMath::IsFinite(FireFieldDuration) || FireFieldDuration <= 0.0f || !FMath::IsFinite(DamageInterval) || DamageInterval <= 0.0f)
	{
		SetValidationError(TEXT("Fire field radius, duration, and damage interval must be greater than zero."));

		return false;
	}

	if (SpecialPatternCleanupOffset <= 0)
	{
		SetValidationError(TEXT("SpecialPatternCleanupOffset must be greater than zero."));

		return false;
	}

	return true;
}

ARSBossFlameCrater::ARSBossFlameCrater()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	CapsuleComp = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CapsuleComponent"));
	SetRootComponent(CapsuleComp);
	CapsuleComp->InitCapsuleSize(45.0f, 90.0f);
	CapsuleComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CapsuleComp->SetCollisionObjectType(ECC_Pawn);
	CapsuleComp->SetCollisionResponseToAllChannels(ECR_Ignore);
	CapsuleComp->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Overlap);

	VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
	VisualRoot->SetupAttachment(CapsuleComp);

	FlameCraterMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FlameCraterMeshComponent"));
	FlameCraterMeshComp->SetupAttachment(VisualRoot);
	FlameCraterMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FlameCraterMeshComp->SetReceivesDecals(false);

	FallingNiagaraComp = CreateDefaultSubobject<UNiagaraComponent>(TEXT("FallingNiagaraComponent"));
	FallingNiagaraComp->SetupAttachment(VisualRoot);
	FallingNiagaraComp->SetAutoActivate(false);

	FireFieldNiagaraComp = CreateDefaultSubobject<UNiagaraComponent>(TEXT("FireFieldNiagaraComponent"));
	FireFieldNiagaraComp->SetupAttachment(CapsuleComp);
	FireFieldNiagaraComp->SetAutoActivate(false);

	ChargeWidgetComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("ChargeWidgetComponent"));
	ChargeWidgetComp->SetupAttachment(CapsuleComp);
	ChargeWidgetComp->SetWidgetSpace(EWidgetSpace::Screen);
	ChargeWidgetComp->SetDrawAtDesiredSize(true);
	ChargeWidgetComp->SetVisibility(false);

	AbilitySystemComp = CreateDefaultSubobject<URSAbilitySystemComponent>(TEXT("RSAbilitySystemComponent"));
	HealthSet = CreateDefaultSubobject<URSHealthSet>(TEXT("RSHealthSet"));
	HealthComp = CreateDefaultSubobject<URSHealthComponent>(TEXT("HealthComponent"));
	PersistentObjectLifetimeComp = CreateDefaultSubobject<URSBossPersistentObjectLifetimeComponent>(TEXT("PersistentObjectLifetimeComponent"));
}

void ARSBossFlameCrater::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	switch (FlameCraterState)
	{
	case ERSBossFlameCraterState::Falling:
		AdvanceFalling(DeltaSeconds);
		break;

	case ERSBossFlameCraterState::Charging:
		AdvanceCharging(DeltaSeconds);
		break;

	default:
		SetActorTickEnabled(false);
		break;
	}
}

void ARSBossFlameCrater::BeginPlay()
{
	Super::BeginPlay();

	InitializeAbilitySystem();
	if (PersistentObjectLifetimeComp)
	{
		PersistentObjectLifetimeComp->OnCleanupRequired().AddUObject(this, &ThisClass::RequestCleanup);
		PersistentObjectLifetimeComp->StartObserving(GetOwner(), FlameCraterDefinition.SpecialPatternCleanupOffset);
	}

	if (FallingNiagaraComp)
	{
		FallingNiagaraComp->SetAsset(FallingNiagaraSystem);
	}

	if (FireFieldNiagaraComp)
	{
		FireFieldNiagaraComp->SetAsset(FireFieldNiagaraSystem);
		UpdateFireFieldNiagaraScale();
	}

	if (ChargeWidgetComp)
	{
		ChargeWidgetComp->SetWidgetClass(ChargeWidgetClass);
	}

	if (!FlameCraterDefinition.IsDataValid())
	{
		RequestCleanup();

		return;
	}

	BeginFalling();
}

void ARSBossFlameCrater::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearTimers();
	if (PersistentObjectLifetimeComp)
	{
		PersistentObjectLifetimeComp->OnCleanupRequired().RemoveAll(this);
		PersistentObjectLifetimeComp->StopObserving();
	}

	if (AbilitySystemComp)
	{
		AbilitySystemComp->RemoveLooseGameplayTag(RSGameplayTags::State_Hazard_FlameCrater_Vulnerable);
		AbilitySystemComp->CancelAbilities();
		GrantedAbilityHandles.TakeFromAbilitySystem(AbilitySystemComp);
	}

	if (HealthComp)
	{
		HealthComp->OnDeathStarted.RemoveDynamic(this, &ThisClass::HandleDeathStarted);
	}

	Super::EndPlay(EndPlayReason);
}

#if WITH_EDITOR
EDataValidationResult ARSBossFlameCrater::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult ValidationResult = Super::IsDataValid(Context);

	FString ValidationError;
	if (!FlameCraterDefinition.IsDataValid(&ValidationError))
	{
		Context.AddError(FText::FromString(FString::Printf(TEXT("FlameCraterDefinition is invalid: %s"), *ValidationError)));
		ValidationResult = EDataValidationResult::Invalid;
	}

	if (!DefaultAbilitySet)
	{
		Context.AddError(FText::FromString(TEXT("DefaultAbilitySet is not configured.")));
		ValidationResult = EDataValidationResult::Invalid;
	}

	if (!ChargeWidgetClass)
	{
		Context.AddError(FText::FromString(TEXT("ChargeWidgetClass is not configured.")));
		ValidationResult = EDataValidationResult::Invalid;
	}

	if (!FMath::IsFinite(FireFieldNiagaraBaseRadius) || FireFieldNiagaraBaseRadius <= 0.0f)
	{
		Context.AddError(FText::FromString(TEXT("FireFieldNiagaraBaseRadius must be a finite value greater than zero.")));
		ValidationResult = EDataValidationResult::Invalid;
	}

	return ValidationResult;
}
#endif

UAbilitySystemComponent* ARSBossFlameCrater::GetAbilitySystemComponent() const
{
	return AbilitySystemComp;
}

void ARSBossFlameCrater::InitializeAbilitySystem()
{
	if (!AbilitySystemComp)
	{
		return;
	}

	AbilitySystemComp->InitAbilityActorInfo(this, this);

	if (AbilitySystemComp->IsOwnerActorAuthoritative() && DefaultAbilitySet)
	{
		DefaultAbilitySet->GiveToAbilitySystem(AbilitySystemComp, &GrantedAbilityHandles, this);
	}

	AbilitySystemComp->SetNumericAttributeBase(URSHealthSet::GetMaxHealthAttribute(), FlameCraterDefinition.RequiredHitCount);
	AbilitySystemComp->SetNumericAttributeBase(URSHealthSet::GetHealthAttribute(), FlameCraterDefinition.RequiredHitCount);

	if (HealthComp)
	{
		HealthComp->OnDeathStarted.AddUniqueDynamic(this, &ThisClass::HandleDeathStarted);
		HealthComp->InitializeWithAbilitySystem(AbilitySystemComp);
	}
}

void ARSBossFlameCrater::HandleDeathStarted(URSHealthComponent* InHealthComponent)
{
	if (InHealthComponent == HealthComp)
	{
		RequestCleanup();
	}
}

void ARSBossFlameCrater::RequestCleanup()
{
	if (FlameCraterState == ERSBossFlameCraterState::Cleanup)
	{
		return;
	}

	FlameCraterState = ERSBossFlameCraterState::Cleanup;
	SetActorTickEnabled(false);
	ClearTimers();

	if (CapsuleComp)
	{
		CapsuleComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (ChargeWidgetComp)
	{
		ChargeWidgetComp->SetVisibility(false);
	}

	if (FallingNiagaraComp)
	{
		FallingNiagaraComp->DeactivateImmediate();
	}

	if (FireFieldNiagaraComp)
	{
		FireFieldNiagaraComp->DeactivateImmediate();
	}

	if (AbilitySystemComp)
	{
		AbilitySystemComp->RemoveLooseGameplayTag(RSGameplayTags::State_Hazard_FlameCrater_Vulnerable);
		AbilitySystemComp->CancelAbilities();
	}

	Destroy();
}

void ARSBossFlameCrater::BeginFalling()
{
	FlameCraterState = ERSBossFlameCraterState::Falling;
	StateElapsedTime = 0.0f;
	VisualRoot->SetRelativeLocation(FVector::UpVector * FlameCraterDefinition.FallHeight);
	CapsuleComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ChargeWidgetComp->SetVisibility(false);

	if (FallingNiagaraSystem)
	{
		FallingNiagaraComp->Activate(true);
	}

	SetActorTickEnabled(true);
}

void ARSBossFlameCrater::AdvanceFalling(float DeltaSeconds)
{
	StateElapsedTime += DeltaSeconds;
	const float LinearProgress = FMath::Clamp(StateElapsedTime / FlameCraterDefinition.FallDuration, 0.0f, 1.0f);
	const float CurveProgress = FlameCraterDefinition.FallCurve ? FlameCraterDefinition.FallCurve->GetFloatValue(LinearProgress) : LinearProgress;
	const float Height = FMath::Lerp(FlameCraterDefinition.FallHeight, 0.0f, FMath::Clamp(CurveProgress, 0.0f, 1.0f));
	VisualRoot->SetRelativeLocation(FVector::UpVector * Height);

	if (LinearProgress >= 1.0f)
	{
		VisualRoot->SetRelativeLocation(FVector::ZeroVector);
		BeginCharging();
	}
}

void ARSBossFlameCrater::BeginCharging()
{
	FlameCraterState = ERSBossFlameCraterState::Charging;
	StateElapsedTime = 0.0f;
	FallingNiagaraComp->Deactivate();
	CapsuleComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ChargeWidgetComp->SetVisibility(true);
	UpdateChargeWidget(0.0f);
	AbilitySystemComp->AddLooseGameplayTag(RSGameplayTags::State_Hazard_FlameCrater_Vulnerable);
	SetActorTickEnabled(true);
}

void ARSBossFlameCrater::AdvanceCharging(float DeltaSeconds)
{
	StateElapsedTime += DeltaSeconds;
	const float Progress = FMath::Clamp(StateElapsedTime / FlameCraterDefinition.ChargeDuration, 0.0f, 1.0f);
	UpdateChargeWidget(Progress);

	if (Progress >= 1.0f)
	{
		RequestFireFieldTransition();
	}
}

void ARSBossFlameCrater::RequestFireFieldTransition()
{
	if (FlameCraterState != ERSBossFlameCraterState::Charging)
	{
		return;
	}

	FlameCraterState = ERSBossFlameCraterState::ChargeCompletedPending;
	ChargeCompletionFrame = GFrameCounter;
	SetActorTickEnabled(false);

	if (UWorld* World = GetWorld())
	{
		ChargeCompletionTimerHandle = World->GetTimerManager().SetTimerForNextTick(this, &ThisClass::CompleteFireFieldTransition);
	}
}

void ARSBossFlameCrater::CompleteFireFieldTransition()
{
	if (FlameCraterState != ERSBossFlameCraterState::ChargeCompletedPending)
	{
		return;
	}

	// UE 5.8은 TimerManager Tick 전 요청한 SetTimerForNextTick을 같은 Frame에 실행할 수 있어 실제 Frame 진행을 확인합니다
	if (GFrameCounter <= ChargeCompletionFrame)
	{
		if (UWorld* World = GetWorld())
		{
			ChargeCompletionTimerHandle = World->GetTimerManager().SetTimerForNextTick(this, &ThisClass::CompleteFireFieldTransition);
		}

		return;
	}

	BeginFireField();
}

void ARSBossFlameCrater::BeginFireField()
{
	if (FlameCraterState != ERSBossFlameCraterState::ChargeCompletedPending)
	{
		return;
	}

	FlameCraterState = ERSBossFlameCraterState::FireField;
	AbilitySystemComp->RemoveLooseGameplayTag(RSGameplayTags::State_Hazard_FlameCrater_Vulnerable);
	CapsuleComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ChargeWidgetComp->SetVisibility(false);
	UpdateFireFieldNiagaraScale();

	if (FireFieldNiagaraSystem)
	{
		FireFieldNiagaraComp->Activate(true);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(FireFieldDamageTimerHandle, this, &ThisClass::ApplyFireFieldDamage, FlameCraterDefinition.DamageInterval, true, FlameCraterDefinition.DamageInterval);
		World->GetTimerManager().SetTimer(FireFieldLifetimeTimerHandle, this, &ThisClass::RequestCleanup, FlameCraterDefinition.FireFieldDuration, false);
	}
}

void ARSBossFlameCrater::UpdateFireFieldNiagaraScale()
{
	if (!FireFieldNiagaraComp || !FMath::IsFinite(FireFieldNiagaraBaseRadius) || FireFieldNiagaraBaseRadius <= 0.0f)
	{
		return;
	}

	const float HorizontalScale = FlameCraterDefinition.FireFieldRadius / FireFieldNiagaraBaseRadius;
	FireFieldNiagaraComp->SetRelativeScale3D(FVector(HorizontalScale, HorizontalScale, 1.0f));
}

void ARSBossFlameCrater::ApplyFireFieldDamage()
{
	if (FlameCraterState != ERSBossFlameCraterState::FireField)
	{
		return;
	}

	FRSCombatShape FireFieldShape;
	FireFieldShape.Type = ERSCombatShapeType::Sphere;
	FireFieldShape.Radius = FlameCraterDefinition.FireFieldRadius;
	FireFieldShape.InnerRadius = 0.0f;

	TArray<AActor*> HitTargets;
	URSCombatFunctionLibrary::FindTargetsInShape(this, PlayerTargetChannel, FireFieldShape, GetActorTransform(), HitTargets);
	for (AActor* HitTarget : HitTargets)
	{
		if (!Cast<ARSPlayerCharacter>(HitTarget))
		{
			continue;
		}

		FGameplayEventData DamagePayload;
		DamagePayload.EventTag = RSGameplayTags::GameplayEvent_Combat_FireFieldDamage;
		DamagePayload.Instigator = this;
		DamagePayload.Target = HitTarget;
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(this, DamagePayload.EventTag, DamagePayload);
	}
}

void ARSBossFlameCrater::UpdateChargeWidget(float Progress)
{
	if (!ChargeWidgetComp)
	{
		return;
	}

	if (URSFlameCraterChargeWidget* ChargeWidget = Cast<URSFlameCraterChargeWidget>(ChargeWidgetComp->GetUserWidgetObject()))
	{
		ChargeWidget->SetChargeProgress(Progress);
	}
}

void ARSBossFlameCrater::ClearTimers()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ChargeCompletionTimerHandle);
		World->GetTimerManager().ClearTimer(FireFieldDamageTimerHandle);
		World->GetTimerManager().ClearTimer(FireFieldLifetimeTimerHandle);
	}

	ChargeCompletionTimerHandle.Invalidate();
	FireFieldDamageTimerHandle.Invalidate();
	FireFieldLifetimeTimerHandle.Invalidate();
}
