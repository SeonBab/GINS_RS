#include "RSBossFireball.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/WidgetComponent.h"
#include "Combat/RSCombatFunctionLibrary.h"
#include "Curves/CurveFloat.h"
#include "Engine/World.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "RSAbilitySystemComponent.h"
#include "RSBossPersistentObjectLifetimeComponent.h"
#include "RSFireballChargeWidget.h"
#include "RSGameplayTags.h"
#include "RSHealthComponent.h"
#include "RSHealthSet.h"
#include "RSPlayerCharacter.h"
#include "TimerManager.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

bool FRSBossFireballDefinition::IsDataValid(FString* OutValidationError) const
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

ARSBossFireball::ARSBossFireball()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// 화염구는 Mesh 없이 Niagara로만 보이므로 충돌은 이 박스 하나가 전담합니다
	// 맵 지오메트리는 Block하고 플레이어와 보스는 통과시켜 이동을 막지 않습니다
	BoxComp = CreateDefaultSubobject<UBoxComponent>(TEXT("BoxComponent"));
	SetRootComponent(BoxComp);
	BoxComp->SetBoxExtent(FVector(45.0f, 45.0f, 45.0f));
	BoxComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BoxComp->SetCollisionObjectType(ECC_WorldDynamic);
	BoxComp->SetCollisionResponseToAllChannels(ECR_Ignore);
	BoxComp->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	BoxComp->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	BoxComp->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Overlap);

	// 플레이어가 통과할 수 있는 오브젝트이므로 NavMesh를 깎지 않습니다
	// 깎으면 클릭 이동 경로만 화염구를 피해 돌아가 실제로 지나갈 수 있는 것과 어긋납니다
	BoxComp->SetCanEverAffectNavigation(false);

	// 충돌 박스는 착지 지점에 고정되고 낙하는 이 VisualRoot만 내려옵니다
	VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
	VisualRoot->SetupAttachment(BoxComp);

	FireballNiagaraComp = CreateDefaultSubobject<UNiagaraComponent>(TEXT("FireballNiagaraComponent"));
	FireballNiagaraComp->SetupAttachment(VisualRoot);
	FireballNiagaraComp->SetAutoActivate(false);

	FireFieldNiagaraComp = CreateDefaultSubobject<UNiagaraComponent>(TEXT("FireFieldNiagaraComponent"));
	FireFieldNiagaraComp->SetupAttachment(BoxComp);
	FireFieldNiagaraComp->SetAutoActivate(false);

	ChargeWidgetComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("ChargeWidgetComponent"));
	ChargeWidgetComp->SetupAttachment(BoxComp);
	ChargeWidgetComp->SetWidgetSpace(EWidgetSpace::Screen);
	ChargeWidgetComp->SetDrawAtDesiredSize(true);
	ChargeWidgetComp->SetVisibility(false);

	AbilitySystemComp = CreateDefaultSubobject<URSAbilitySystemComponent>(TEXT("RSAbilitySystemComponent"));
	HealthSet = CreateDefaultSubobject<URSHealthSet>(TEXT("RSHealthSet"));
	HealthComp = CreateDefaultSubobject<URSHealthComponent>(TEXT("HealthComponent"));
	PersistentObjectLifetimeComp = CreateDefaultSubobject<URSBossPersistentObjectLifetimeComponent>(TEXT("PersistentObjectLifetimeComponent"));
}

void ARSBossFireball::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	switch (FireballState)
	{
	case ERSBossFireballState::Falling:
		AdvanceFalling(DeltaSeconds);
		break;

	case ERSBossFireballState::Charging:
		AdvanceCharging(DeltaSeconds);
		break;

	default:
		SetActorTickEnabled(false);
		break;
	}
}

void ARSBossFireball::BeginPlay()
{
	Super::BeginPlay();

	InitializeAbilitySystem();
	PersistentObjectLifetimeComp->OnCleanupRequired().AddUObject(this, &ThisClass::RequestCleanup);
	PersistentObjectLifetimeComp->StartObserving(GetOwner(), FireballDefinition.SpecialPatternCleanupOffset);
	FireballNiagaraComp->SetAsset(FireballNiagaraSystem);
	FireFieldNiagaraComp->SetAsset(FireFieldNiagaraSystem);
	UpdateFireFieldNiagaraScale();
	ChargeWidgetComp->SetWidgetClass(ChargeWidgetClass);

	if (!FireballDefinition.IsDataValid())
	{
		RequestCleanup();

		return;
	}

	BeginFalling();
}

void ARSBossFireball::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearTimers();
	PersistentObjectLifetimeComp->OnCleanupRequired().RemoveAll(this);
	PersistentObjectLifetimeComp->StopObserving();
	AbilitySystemComp->RemoveLooseGameplayTag(RSGameplayTags::State_Hazard_Fireball_Vulnerable);
	AbilitySystemComp->CancelAbilities();
	GrantedAbilityHandles.TakeFromAbilitySystem(AbilitySystemComp);
	HealthComp->OnDeathStarted.RemoveDynamic(this, &ThisClass::HandleDeathStarted);
	FireballNiagaraComp->DeactivateImmediate();
	FireFieldNiagaraComp->DeactivateImmediate();

	Super::EndPlay(EndPlayReason);
}

#if WITH_EDITOR
EDataValidationResult ARSBossFireball::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult ValidationResult = Super::IsDataValid(Context);

	FString ValidationError;
	if (!FireballDefinition.IsDataValid(&ValidationError))
	{
		Context.AddError(FText::FromString(FString::Printf(TEXT("FireballDefinition is invalid: %s"), *ValidationError)));
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

UAbilitySystemComponent* ARSBossFireball::GetAbilitySystemComponent() const
{
	return AbilitySystemComp;
}

void ARSBossFireball::InitializeAbilitySystem()
{
	AbilitySystemComp->InitAbilityActorInfo(this, this);

	if (AbilitySystemComp->IsOwnerActorAuthoritative() && DefaultAbilitySet)
	{
		DefaultAbilitySet->GiveToAbilitySystem(AbilitySystemComp, &GrantedAbilityHandles, this);
	}

	AbilitySystemComp->SetNumericAttributeBase(URSHealthSet::GetMaxHealthAttribute(), FireballDefinition.RequiredHitCount);
	AbilitySystemComp->SetNumericAttributeBase(URSHealthSet::GetHealthAttribute(), FireballDefinition.RequiredHitCount);

	HealthComp->OnDeathStarted.AddUniqueDynamic(this, &ThisClass::HandleDeathStarted);
	HealthComp->InitializeWithAbilitySystem(AbilitySystemComp);
}

void ARSBossFireball::HandleDeathStarted(URSHealthComponent* InHealthComponent)
{
	RequestCleanup();
}

void ARSBossFireball::RequestCleanup()
{
	if (FireballState == ERSBossFireballState::Cleanup)
	{
		return;
	}

	// Destroy()가 EndPlay()를 동기적으로 호출하므로 Timer, 태그, Ability와 Niagara 회수는 EndPlay() 한 경로에서만 수행합니다
	FireballState = ERSBossFireballState::Cleanup;
	SetActorTickEnabled(false);
	Destroy();
}

void ARSBossFireball::BeginFalling()
{
	FireballState = ERSBossFireballState::Falling;
	StateElapsedTime = 0.0f;
	VisualRoot->SetRelativeLocation(FVector::UpVector * FireballDefinition.FallHeight);
	BoxComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ChargeWidgetComp->SetVisibility(false);

	if (FireballNiagaraSystem)
	{
		FireballNiagaraComp->Activate(true);
	}

	SetActorTickEnabled(true);
}

void ARSBossFireball::AdvanceFalling(float DeltaSeconds)
{
	StateElapsedTime += DeltaSeconds;
	const float LinearProgress = FMath::Clamp(StateElapsedTime / FireballDefinition.FallDuration, 0.0f, 1.0f);
	const float CurveProgress = FireballDefinition.FallCurve ? FireballDefinition.FallCurve->GetFloatValue(LinearProgress) : LinearProgress;
	const float Height = FMath::Lerp(FireballDefinition.FallHeight, 0.0f, FMath::Clamp(CurveProgress, 0.0f, 1.0f));
	VisualRoot->SetRelativeLocation(FVector::UpVector * Height);

	if (LinearProgress >= 1.0f)
	{
		VisualRoot->SetRelativeLocation(FVector::ZeroVector);
		BeginCharging();
	}
}

void ARSBossFireball::BeginCharging()
{
	FireballState = ERSBossFireballState::Charging;
	StateElapsedTime = 0.0f;

	// 화염구 본체 Niagara는 충전 중에도 계속 보여야 하므로 착지했다고 끄지 않습니다
	// 낙하 연출과 본체가 같은 Component이며, 끄는 시점은 장판으로 전환할 때 하나뿐입니다
	BoxComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ChargeWidgetComp->SetVisibility(true);
	UpdateChargeWidget(0.0f);
	AbilitySystemComp->AddLooseGameplayTag(RSGameplayTags::State_Hazard_Fireball_Vulnerable);
	SetActorTickEnabled(true);
}

void ARSBossFireball::AdvanceCharging(float DeltaSeconds)
{
	StateElapsedTime += DeltaSeconds;
	const float Progress = FMath::Clamp(StateElapsedTime / FireballDefinition.ChargeDuration, 0.0f, 1.0f);
	UpdateChargeWidget(Progress);

	if (Progress >= 1.0f)
	{
		RequestFireFieldTransition();
	}
}

void ARSBossFireball::RequestFireFieldTransition()
{
	if (FireballState != ERSBossFireballState::Charging)
	{
		return;
	}

	FireballState = ERSBossFireballState::ChargeCompletedPending;
	ChargeCompletionFrame = GFrameCounter;
	SetActorTickEnabled(false);

	if (UWorld* World = GetWorld())
	{
		ChargeCompletionTimerHandle = World->GetTimerManager().SetTimerForNextTick(this, &ThisClass::CompleteFireFieldTransition);
	}
}

void ARSBossFireball::CompleteFireFieldTransition()
{
	if (FireballState != ERSBossFireballState::ChargeCompletedPending)
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

void ARSBossFireball::BeginFireField()
{
	FireballState = ERSBossFireballState::FireField;
	AbilitySystemComp->RemoveLooseGameplayTag(RSGameplayTags::State_Hazard_Fireball_Vulnerable);

	// 장판이 되는 순간 화염구 본체는 사라지고 장판 Niagara가 그 자리를 대신합니다
	FireballNiagaraComp->Deactivate();
	BoxComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ChargeWidgetComp->SetVisibility(false);
	UpdateFireFieldNiagaraScale();

	if (FireFieldNiagaraSystem)
	{
		FireFieldNiagaraComp->Activate(true);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(FireFieldDamageTimerHandle, this, &ThisClass::ApplyFireFieldDamage, FireballDefinition.DamageInterval, true, FireballDefinition.DamageInterval);
		World->GetTimerManager().SetTimer(FireFieldLifetimeTimerHandle, this, &ThisClass::RequestCleanup, FireballDefinition.FireFieldDuration, false);
	}
}

void ARSBossFireball::UpdateFireFieldNiagaraScale()
{
	if (!FMath::IsFinite(FireFieldNiagaraBaseRadius) || FireFieldNiagaraBaseRadius <= 0.0f)
	{
		return;
	}

	const float HorizontalScale = FireballDefinition.FireFieldRadius / FireFieldNiagaraBaseRadius;
	FireFieldNiagaraComp->SetRelativeScale3D(FVector(HorizontalScale, HorizontalScale, 1.0f));
}

void ARSBossFireball::ApplyFireFieldDamage()
{
	if (FireballState != ERSBossFireballState::FireField)
	{
		return;
	}

	FRSCombatShape FireFieldShape;
	FireFieldShape.Type = ERSCombatShapeType::Sphere;
	FireFieldShape.Radius = FireballDefinition.FireFieldRadius;
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

void ARSBossFireball::UpdateChargeWidget(float Progress)
{
	if (URSFireballChargeWidget* ChargeWidget = Cast<URSFireballChargeWidget>(ChargeWidgetComp->GetUserWidgetObject()))
	{
		ChargeWidget->SetChargeProgress(Progress);
	}
}

void ARSBossFireball::ClearTimers()
{
	// FTimerManager::ClearTimer()가 Handle을 함께 Invalidate하므로 별도 처리는 두지 않습니다
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ChargeCompletionTimerHandle);
		World->GetTimerManager().ClearTimer(FireFieldDamageTimerHandle);
		World->GetTimerManager().ClearTimer(FireFieldLifetimeTimerHandle);
	}
}
