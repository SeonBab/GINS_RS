#include "RSAbilityTask_ScanInteractables.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "RSInteractionSelection.h"

URSAbilityTask_ScanInteractables* URSAbilityTask_ScanInteractables::ScanInteractables(UGameplayAbility* OwningAbility, float ScanRadius, float ScanInterval, ECollisionChannel ScanChannel)
{
	URSAbilityTask_ScanInteractables* Task = NewAbilityTask<URSAbilityTask_ScanInteractables>(OwningAbility);
	Task->ScanRadius = ScanRadius;
	Task->ScanInterval = ScanInterval;
	Task->ScanChannel = ScanChannel;

	return Task;
}

void URSAbilityTask_ScanInteractables::Activate()
{
	Super::Activate();

	UWorld* World = GetWorld();
	if (!World || ScanRadius <= 0.0f || ScanInterval <= 0.0f || ScanChannel == ECollisionChannel::ECC_MAX)
	{
		EndTask();

		return;
	}

	// 탐색을 시작한 위치에 이미 대상이 있을 수 있으므로 첫 주기를 기다리지 않고 한 번 훑습니다
	PerformScan();

	World->GetTimerManager().SetTimer(ScanTimerHandle, this, &ThisClass::PerformScan, ScanInterval, true);
}

void URSAbilityTask_ScanInteractables::OnDestroy(bool bInOwnerFinished)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ScanTimerHandle);
	}

	Super::OnDestroy(bInOwnerFinished);
}

void URSAbilityTask_ScanInteractables::PerformScan()
{
	const AActor* AvatarActor = GetAvatarActor();
	UWorld* World = GetWorld();

	if (!IsValid(AvatarActor) || !World)
	{
		return;
	}

	const FVector ScanOrigin = AvatarActor->GetActorLocation();

	TArray<FOverlapResult> OverlapResults;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RSScanInteractables), false, AvatarActor);

	World->OverlapMultiByChannel(OverlapResults, ScanOrigin, FQuat::Identity, ScanChannel, FCollisionShape::MakeSphere(ScanRadius), QueryParams);

	TArray<UPrimitiveComponent*> CandidateComponents;
	CandidateComponents.Reserve(OverlapResults.Num());

	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		CandidateComponents.Add(OverlapResult.GetComponent());
	}

	AActor* NearestInteractable = RSInteraction::FindNearestInteractable(CandidateComponents, ScanOrigin, AvatarActor);
	if (NearestInteractable == FocusedActor.Get())
	{
		return;
	}

	FocusedActor = NearestInteractable;

	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnFocusChanged.Broadcast(NearestInteractable);
	}
}
