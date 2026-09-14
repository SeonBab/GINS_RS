#include "RSTeleporter.h"

#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "RSPlayerCharacter.h"

ARSTeleporter::ARSTeleporter()
{
	PrimaryActorTick.bCanEverTick = false;

	TriggerArea = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerArea"));
	SetRootComponent(TriggerArea);

	TriggerArea->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerArea->SetCollisionObjectType(ECC_WorldDynamic);
	TriggerArea->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerArea->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerArea->SetGenerateOverlapEvents(true);

	// UArrowComponent는 Editor 표시 전용이며 위치와 함께 도착 방향까지 뷰포트에서 확인할 수 있습니다
	DestinationPoint = CreateDefaultSubobject<UArrowComponent>(TEXT("DestinationPoint"));
	DestinationPoint->SetupAttachment(TriggerArea);
}

void ARSTeleporter::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		// 이동은 서버에서만 수행하고 클라이언트에는 Pawn의 위치가 복제됩니다
		return;
	}

	TriggerArea->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleTriggerAreaBeginOverlap);
}

void ARSTeleporter::HandleTriggerAreaBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	ARSPlayerCharacter* PlayerCharacter = Cast<ARSPlayerCharacter>(OtherActor);
	if (!PlayerCharacter)
	{
		return;
	}

	// 도착 지점의 Pitch와 Roll은 캐릭터를 기울이므로 Yaw만 사용합니다
	const FRotator DestinationRotation(0.0f, DestinationPoint->GetComponentRotation().Yaw, 0.0f);
	if (!PlayerCharacter->TeleportTo(DestinationPoint->GetComponentLocation(), DestinationRotation))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s가 %s를 도착 지점으로 옮기지 못했습니다"), *GetName(), *PlayerCharacter->GetName());
		return;
	}

	// 남은 경로 추종은 도착한 캐릭터를 이전 목적지로 다시 이동시키고 그 과정에서 도착 방향까지 덮어씁니다
	PlayerCharacter->StopNavigationMovement();
}
