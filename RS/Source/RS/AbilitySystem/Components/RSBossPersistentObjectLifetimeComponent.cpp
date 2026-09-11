#include "RSBossPersistentObjectLifetimeComponent.h"

#include "RSBossPhaseComponent.h"

URSBossPersistentObjectLifetimeComponent::URSBossPersistentObjectLifetimeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void URSBossPersistentObjectLifetimeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopObserving();

	Super::EndPlay(EndPlayReason);
}

bool URSBossPersistentObjectLifetimeComponent::StartObserving(AActor* BossActor, int32 InSpecialPatternCleanupOffset)
{
	StopObserving();

	if (!IsValid(BossActor) || InSpecialPatternCleanupOffset <= 0)
	{
		return false;
	}

	URSBossPhaseComponent* PhaseComponent = BossActor->FindComponentByClass<URSBossPhaseComponent>();
	if (!PhaseComponent)
	{
		return false;
	}

	BossPhaseComp = PhaseComponent;
	SpawnSpecialPatternSequence = PhaseComponent->GetSpecialPatternActivationSequence();
	SpecialPatternCleanupOffset = InSpecialPatternCleanupOffset;
	SpecialPatternActivationRequestedDelegateHandle = PhaseComponent->OnSpecialPatternActivationRequested().AddUObject(this, &ThisClass::HandleSpecialPatternActivationRequested);
	PersistentObjectCleanupDelegateHandle = PhaseComponent->OnPersistentObjectCleanupRequested().AddUObject(this, &ThisClass::HandlePersistentObjectCleanupRequested);

	return true;
}

void URSBossPersistentObjectLifetimeComponent::StopObserving()
{
	URSBossPhaseComponent* PhaseComponent = BossPhaseComp.Get();
	if (PhaseComponent)
	{
		PhaseComponent->OnSpecialPatternActivationRequested().Remove(SpecialPatternActivationRequestedDelegateHandle);
		PhaseComponent->OnPersistentObjectCleanupRequested().Remove(PersistentObjectCleanupDelegateHandle);
	}

	BossPhaseComp.Reset();
	SpecialPatternActivationRequestedDelegateHandle.Reset();
	PersistentObjectCleanupDelegateHandle.Reset();
	SpawnSpecialPatternSequence = 0;
	SpecialPatternCleanupOffset = 0;
}

bool URSBossPersistentObjectLifetimeComponent::ShouldCleanupForSpecialPattern(int32 SpawnSequence, int32 ActiveSequence, int32 InSpecialPatternCleanupOffset)
{
	const int64 CleanupSequence = static_cast<int64>(SpawnSequence) + InSpecialPatternCleanupOffset;

	return SpawnSequence >= 0 && InSpecialPatternCleanupOffset > 0 && ActiveSequence >= CleanupSequence;
}

void URSBossPersistentObjectLifetimeComponent::RequestCleanup()
{
	if (!IsObserving())
	{
		return;
	}

	StopObserving();
	CleanupRequiredEvent.Broadcast();
}

void URSBossPersistentObjectLifetimeComponent::HandleSpecialPatternActivationRequested(int32 ActiveSequence)
{
	if (ShouldCleanupForSpecialPattern(SpawnSpecialPatternSequence, ActiveSequence, SpecialPatternCleanupOffset))
	{
		RequestCleanup();
	}
}

void URSBossPersistentObjectLifetimeComponent::HandlePersistentObjectCleanupRequested()
{
	RequestCleanup();
}
