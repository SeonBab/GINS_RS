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

	// 레벨에서 옮기는 대상은 눈에 보이는 본체이므로 감지 영역의 크기를 바꿔도 배치 기준이 흔들리지 않게 본체를 Root로 둡니다
	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	SetRootComponent(BodyMesh);

	ChargedMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChargedMesh"));
	ChargedMesh->SetupAttachment(BodyMesh);

	// 충전 표시는 상태를 보여줄 뿐이므로 플레이어의 이동이나 판정을 막지 않습니다
	ChargedMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	TriggerArea = CreateDefaultSubobject<USphereComponent>(TEXT("TriggerArea"));
	TriggerArea->SetupAttachment(BodyMesh);

	TriggerArea->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerArea->SetCollisionObjectType(ECC_WorldDynamic);
	TriggerArea->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerArea->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerArea->SetGenerateOverlapEvents(true);
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
	SetCharged(true);

	// 충전이 찬 순간에 영역 안에 있던 대상이 다시 나갔다 들어와야 회복받는 상황을 만들지 않습니다
	TryHealTargetInTriggerArea();
}

bool ARSHealingStructure::TryHealTarget(AActor* TargetActor)
{
	if (!bIsCharged)
	{
		return false;
	}

	const ARSPlayerCharacter* PlayerCharacter = Cast<ARSPlayerCharacter>(TargetActor);
	if (!NeedsHealing(PlayerCharacter))
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

bool ARSHealingStructure::NeedsHealing(const ARSPlayerCharacter* PlayerCharacter)
{
	if (!PlayerCharacter)
	{
		return false;
	}

	const URSHealthComponent* HealthComp = PlayerCharacter->GetHealthComponent();
	if (!HealthComp || HealthComp->IsDead())
	{
		return false;
	}

	// 체력이 가득 찬 대상까지 회복시키면 다치지 않은 플레이어가 서 있는 것만으로 충전이 계속 소모됩니다
	return HealthComp->GetHealth() < HealthComp->GetMaxHealth();
}

void ARSHealingStructure::SetCharged(bool bInCharged)
{
	bIsCharged = bInCharged;

	ChargedMesh->SetVisibility(bInCharged, true);
}

void ARSHealingStructure::StartRecharge()
{
	SetCharged(false);

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
#endif
