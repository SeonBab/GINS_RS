#include "RSHealingStructure.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "RSGameplayTags.h"
#include "RSHealthComponent.h"
#include "RSPlayerCharacter.h"
#include "TimerManager.h"

ARSHealingStructure::ARSHealingStructure()
{
	PrimaryActorTick.bCanEverTick = false;

	TriggerArea = CreateDefaultSubobject<USphereComponent>(TEXT("TriggerArea"));
	SetRootComponent(TriggerArea);

	TriggerArea->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerArea->SetCollisionObjectType(ECC_WorldDynamic);
	TriggerArea->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerArea->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerArea->SetGenerateOverlapEvents(true);

	ChargedMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChargedMesh"));
	ChargedMesh->SetupAttachment(TriggerArea);

	// 충전 표시는 상태를 보여줄 뿐이므로 플레이어의 이동이나 판정을 막지 않습니다
	ChargedMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ARSHealingStructure::BeginPlay()
{
	Super::BeginPlay();

	TriggerArea->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleTriggerAreaBeginOverlap);

	if (bStartCharged)
	{
		BecomeCharged();
	}
	else
	{
		StartRecharge();
	}
}

void ARSHealingStructure::HandleTriggerAreaBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	TryHealTarget(OtherActor);
}

void ARSHealingStructure::HandleRechargeCompleted()
{
	BecomeCharged();
}

void ARSHealingStructure::BecomeCharged()
{
	// 표시를 먼저 보여 주고 회복은 지연 뒤에 엽니다
	// 어떤 경로로 소모되든 Mesh가 나타난 뒤 최소한 이 시간만큼은 화면에 남습니다
	ChargedMesh->SetVisibility(true, true);

	if (ChargedHealDelaySeconds <= 0.0f)
	{
		// 0은 Timer가 아예 예약되지 않아 회복이 영영 열리지 않으므로 직접 호출합니다
		HandleChargedHealDelayFinished();

		return;
	}

	GetWorldTimerManager().SetTimer(ChargedHealDelayTimerHandle, this, &ThisClass::HandleChargedHealDelayFinished, ChargedHealDelaySeconds, false);
}

void ARSHealingStructure::HandleChargedHealDelayFinished()
{
	bIsReadyToHeal = true;

	// 회복이 열린 순간에 영역 안에 있던 대상이 다시 나갔다 들어와야 회복받는 상황을 만들지 않습니다
	TryHealTargetInTriggerArea();
}

bool ARSHealingStructure::TryHealTarget(AActor* TargetActor)
{
	if (!bIsReadyToHeal)
	{
		return false;
	}

	const ARSPlayerCharacter* PlayerCharacter = Cast<ARSPlayerCharacter>(TargetActor);
	if (!CanReceiveHealing(PlayerCharacter))
	{
		return false;
	}

	if (!TryApplyHealEffect(PlayerCharacter))
	{
		return false;
	}

	// 충전은 회복이 실제로 일어났을 때만 소모합니다
	StartRecharge();

	return true;
}

bool ARSHealingStructure::TryHealTargetInTriggerArea()
{
	TArray<AActor*> OverlappingActors;
	TriggerArea->GetOverlappingActors(OverlappingActors, ARSPlayerCharacter::StaticClass());

	for (AActor* OverlappingActor : OverlappingActors)
	{
		if (TryHealTarget(OverlappingActor))
		{
			return true;
		}
	}

	return false;
}

bool ARSHealingStructure::TryApplyHealEffect(const ARSPlayerCharacter* TargetPlayerCharacter) const
{
	if (!HealEffectClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s에 HealEffectClass가 없어 회복시키지 못했습니다"), *GetName());

		return false;
	}

	// 구조물은 ASC를 가지지 않으므로 가해자 ASC로 Spec을 만드는 전투 경로 대신 대상 ASC에서 만들어 스스로에게 적용합니다
	UAbilitySystemComponent* TargetAbilitySystemComp = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetPlayerCharacter);
	if (!TargetAbilitySystemComp)
	{
		return false;
	}

	FGameplayEffectContextHandle EffectContext = TargetAbilitySystemComp->MakeEffectContext();
	EffectContext.AddSourceObject(this);

	const FGameplayEffectSpecHandle HealSpecHandle = TargetAbilitySystemComp->MakeOutgoingSpec(HealEffectClass, 1.0f, EffectContext);
	if (!HealSpecHandle.IsValid())
	{
		return false;
	}

	// 공용 회복 GameplayEffect에 회복량을 고정하지 않고 이 구조물의 값을 전달합니다
	HealSpecHandle.Data->SetSetByCallerMagnitude(RSGameplayTags::SetByCaller_Healing, HealAmount);
	TargetAbilitySystemComp->ApplyGameplayEffectSpecToSelf(*HealSpecHandle.Data.Get());

	return true;
}

bool ARSHealingStructure::CanReceiveHealing(const ARSPlayerCharacter* PlayerCharacter)
{
	if (!PlayerCharacter)
	{
		return false;
	}

	const URSHealthComponent* HealthComp = PlayerCharacter->GetHealthComponent();

	// 체력이 가득 찬 대상도 회복시켜 충전을 소모합니다
	// 남겨 두면 영역 안에 서 있는 동안 재충전 타이머가 돌지 않아 그 자리에서 피해를 입어도 회복을 깨울 계기가 없습니다
	// 넘치는 양은 URSHealthSet이 최대 체력으로 자릅니다
	return HealthComp && !HealthComp->IsDead();
}

void ARSHealingStructure::StartRecharge()
{
	bIsReadyToHeal = false;

	ChargedMesh->SetVisibility(false, true);

	// 회복이 열리기 전에 소모될 수는 없지만 시작 시점에 남아 있을 수 있는 예약을 정리합니다
	GetWorldTimerManager().ClearTimer(ChargedHealDelayTimerHandle);

	GetWorldTimerManager().SetTimer(RechargeTimerHandle, this, &ThisClass::HandleRechargeCompleted, RechargeSeconds, false);
}

#if WITH_DEV_AUTOMATION_TESTS
bool ARSHealingStructure::IsRechargeTimerActiveForTest() const
{
	const UWorld* World = GetWorld();

	return World && World->GetTimerManager().IsTimerActive(RechargeTimerHandle);
}

float ARSHealingStructure::GetRechargeRemainingForTest() const
{
	const UWorld* World = GetWorld();

	return World ? World->GetTimerManager().GetTimerRemaining(RechargeTimerHandle) : -1.0f;
}

void ARSHealingStructure::CompleteRechargeForTest()
{
	HandleRechargeCompleted();
}

bool ARSHealingStructure::IsChargedHealDelayActiveForTest() const
{
	const UWorld* World = GetWorld();

	return World && World->GetTimerManager().IsTimerActive(ChargedHealDelayTimerHandle);
}

float ARSHealingStructure::GetChargedHealDelayRemainingForTest() const
{
	const UWorld* World = GetWorld();

	return World ? World->GetTimerManager().GetTimerRemaining(ChargedHealDelayTimerHandle) : -1.0f;
}

void ARSHealingStructure::CompleteChargedHealDelayForTest()
{
	HandleChargedHealDelayFinished();
}
#endif
