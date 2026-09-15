#include "RSTeleporter.h"

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
}

void ARSTeleporter::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		// 이동은 서버에서만 수행하고 클라이언트에는 Pawn의 위치가 복제됩니다
		return;
	}

	if (!DestinationActor)
	{
		// 배치 실수를 첫 Frame에 알리고 목적지가 없는 순간이동은 시도하지 않습니다
		UE_LOG(LogTemp, Warning, TEXT("%s에 DestinationActor가 없어 순간이동을 수행하지 않습니다"), *GetName());
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

	// 목적지의 Pitch와 Roll은 캐릭터를 기울이므로 Yaw만 사용합니다
	const FRotator DestinationRotation(0.0f, DestinationActor->GetActorRotation().Yaw, 0.0f);
	if (!PlayerCharacter->TeleportTo(DestinationActor->GetActorLocation(), DestinationRotation))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s가 %s를 %s 위치로 옮기지 못했습니다"), *GetName(), *PlayerCharacter->GetName(), *DestinationActor->GetName());
	}
}
