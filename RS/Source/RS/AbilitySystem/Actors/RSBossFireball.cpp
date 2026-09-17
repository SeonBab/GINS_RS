#include "RSBossFireball.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/BoxComponent.h"
#include "Components/RSAttackTelegraphComponent.h"
#include "Components/SceneComponent.h"
#include "Components/WidgetComponent.h"
#include "Combat/RSCombatFunctionLibrary.h"
#include "Curves/CurveFloat.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "NiagaraComponent.h"
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

	if (!FMath::IsFinite(FireFieldTelegraphAlpha) || FireFieldTelegraphAlpha <= 0.0f || FireFieldTelegraphAlpha > 1.0f)
	{
		SetValidationError(TEXT("FireFieldTelegraphAlpha must be finite and satisfy 0 < FireFieldTelegraphAlpha <= 1."));

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

	TelegraphComp = CreateDefaultSubobject<URSAttackTelegraphComponent>(TEXT("AttackTelegraphComponent"));

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
	ChargeWidgetComp->SetWidgetClass(ChargeWidgetClass);

	if (!FireballDefinition.IsDataValid() || !HitSpec.IsValid())
	{
		RequestCleanup();

		return;
	}

	BeginFalling();
}

void ARSBossFireball::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearTimers();
	HideTelegraph();
	PersistentObjectLifetimeComp->OnCleanupRequired().RemoveAll(this);
	PersistentObjectLifetimeComp->StopObserving();
	AbilitySystemComp->RemoveLooseGameplayTag(RSGameplayTags::State_Hazard_Fireball_Vulnerable);
	AbilitySystemComp->CancelAbilities();
	GrantedAbilityHandles.TakeFromAbilitySystem(AbilitySystemComp);
	HealthComp->OnDeathStarted.RemoveDynamic(this, &ThisClass::HandleDeathStarted);
	HealthComp->OnHealthChanged.RemoveDynamic(this, &ThisClass::HandleHealthChanged);
	FireballNiagaraComp->DeactivateImmediate();
	FireFieldNiagaraComp->DeactivateImmediate();

	Super::EndPlay(EndPlayReason);
}

#if WITH_EDITOR
EDataValidationResult ARSBossFireball::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult ValidationResult = Super::IsDataValid(Context);

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

	if (!FireballNiagaraComp || !FireballNiagaraComp->GetAsset())
	{
		Context.AddError(FText::FromString(TEXT("FireballNiagaraComponent has no Niagara System asset.")));
		ValidationResult = EDataValidationResult::Invalid;
	}

	if (!FireFieldNiagaraComp || !FireFieldNiagaraComp->GetAsset())
	{
		Context.AddError(FText::FromString(TEXT("FireFieldNiagaraComponent has no Niagara System asset.")));
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

UAbilitySystemComponent* ARSBossFireball::GetAbilitySystemComponent() const
{
	return AbilitySystemComp;
}

void ARSBossFireball::Initialize(const FRSBossFireballDefinition& InFireballDefinition, const FRSBossPatternHitSpec& InHitSpec)
{
	FireballDefinition = InFireballDefinition;
	HitSpec = InHitSpec;
}

void ARSBossFireball::InitializeAbilitySystem()
{
	AbilitySystemComp->InitAbilityActorInfo(this, this);

	if (AbilitySystemComp->IsOwnerActorAuthoritative() && DefaultAbilitySet)
	{
		DefaultAbilitySet->GiveToAbilitySystem(AbilitySystemComp, &GrantedAbilityHandles, this);
	}

	// 필요한 타격 수는 생성 시점에 정해지므로 HealthComponent의 Blueprint 기본값 대신 이 값으로 체력을 초기화합니다
	HealthComp->SetInitialMaxHealth(FireballDefinition.RequiredHitCount);

	HealthComp->OnDeathStarted.AddUniqueDynamic(this, &ThisClass::HandleDeathStarted);
	HealthComp->OnHealthChanged.AddUniqueDynamic(this, &ThisClass::HandleHealthChanged);
	HealthComp->InitializeWithAbilitySystem(AbilitySystemComp);
}

void ARSBossFireball::HandleDeathStarted(URSHealthComponent* InHealthComponent)
{
	RequestCleanup();
}

void ARSBossFireball::HandleHealthChanged(URSHealthComponent* InHealthComponent, float OldValue, float NewValue)
{
	// 충전 중에만 위젯이 보이며 그 밖의 상태에서는 갱신할 칸이 없습니다
	if (FireballState == ERSBossFireballState::Charging)
	{
		UpdateHitPointWidget();
	}
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

	FireballNiagaraComp->Activate(true);

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
	UpdateHitPointWidget();
	AbilitySystemComp->AddLooseGameplayTag(RSGameplayTags::State_Hazard_Fireball_Vulnerable);
	ShowChargeTelegraph();
	SetActorTickEnabled(true);
}

void ARSBossFireball::AdvanceCharging(float DeltaSeconds)
{
	StateElapsedTime += DeltaSeconds;
	const float Progress = FMath::Clamp(StateElapsedTime / FireballDefinition.ChargeDuration, 0.0f, 1.0f);
	UpdateChargeWidget(Progress);
	UpdateChargeTelegraph(Progress);

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
	SwitchTelegraphToFireField();
	FireFieldNiagaraComp->Activate(true);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(FireFieldDamageTimerHandle, this, &ThisClass::ApplyFireFieldDamage, FireballDefinition.DamageInterval, true, FireballDefinition.DamageInterval);
		World->GetTimerManager().SetTimer(FireFieldLifetimeTimerHandle, this, &ThisClass::RequestCleanup, FireballDefinition.FireFieldDuration, false);
	}
}

void ARSBossFireball::ApplyFireFieldDamage()
{
	if (FireballState != ERSBossFireballState::FireField)
	{
		return;
	}

	FRSCombatShape FireFieldShape;
	FireFieldShape.Type = ERSCombatShapeType::AnnularSector;
	FireFieldShape.OuterRadius = FireballDefinition.FireFieldRadius;
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
		DamagePayload.EventMagnitude = HitSpec.DamageAmount;
		DamagePayload.OptionalObject = HitSpec.DamageEffectClass->GetDefaultObject<UGameplayEffect>();
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(this, DamagePayload.EventTag, DamagePayload);
		URSCombatFunctionLibrary::SendHitReaction(this, HitTarget, HitSpec.Reaction);
	}
}

void ARSBossFireball::UpdateChargeWidget(float Progress)
{
	if (URSFireballChargeWidget* ChargeWidget = Cast<URSFireballChargeWidget>(ChargeWidgetComp->GetUserWidgetObject()))
	{
		ChargeWidget->SetChargeProgress(Progress);
	}
}

void ARSBossFireball::UpdateHitPointWidget()
{
	if (URSFireballChargeWidget* ChargeWidget = Cast<URSFireballChargeWidget>(ChargeWidgetComp->GetUserWidgetObject()))
	{
		// 필요 타격 수를 최대 체력으로 두고 피해를 1로 정규화하므로 남은 체력이 곧 남은 타격 수입니다
		ChargeWidget->SetHitPoints(FMath::RoundToInt(HealthComp->GetHealth()), FMath::RoundToInt(HealthComp->GetMaxHealth()));
	}
}

void ARSBossFireball::ShowChargeTelegraph()
{
	if (!TelegraphComp || TelegraphHandle != INDEX_NONE)
	{
		return;
	}

	// 표시와 판정이 같은 형상을 읽으므로 보이는 범위가 장판 판정 반경과 어긋날 수 없습니다
	FRSCombatShape TelegraphShape;
	TelegraphShape.Type = ERSCombatShapeType::AnnularSector;
	TelegraphShape.OuterRadius = FireballDefinition.FireFieldRadius;
	TelegraphShape.InnerRadius = 0.0f;

	// 표시에 실패해도 화염구 자체는 정상 동작해야 하므로 Handle 없이 계속 진행합니다
	// 필수 에셋 누락은 Data Validation이 편집 시점에 막습니다
	TelegraphHandle = TelegraphComp->ShowShapeWithExternalFill(TelegraphShape, FTransform(GetActorLocation()));
}

void ARSBossFireball::UpdateChargeTelegraph(float Progress)
{
	if (TelegraphComp && TelegraphHandle != INDEX_NONE)
	{
		TelegraphComp->SetExternalFill(TelegraphHandle, Progress);
	}
}

void ARSBossFireball::SwitchTelegraphToFireField()
{
	// 충전 중 표시에 실패했더라도 장판 범위는 보여야 하므로 이 시점에 한 번 더 시도합니다
	ShowChargeTelegraph();

	if (!TelegraphComp || TelegraphHandle == INDEX_NONE)
	{
		return;
	}

	// 충전 예고에 쓰던 표시를 그대로 이어 쓰므로 장판이 시작될 때 범위가 비는 프레임이 없습니다
	// 판정이 반복되는 동안에는 채움을 움직이지 않고 불투명도만 낮춰 예고와 구분합니다
	TelegraphComp->SetExternalFill(TelegraphHandle, 1.0f);
	TelegraphComp->SetAlpha(TelegraphHandle, FireballDefinition.FireFieldTelegraphAlpha);
}

void ARSBossFireball::HideTelegraph()
{
	if (TelegraphComp)
	{
		TelegraphComp->HideShape(TelegraphHandle);
	}

	TelegraphHandle = INDEX_NONE;
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
