#include "RSAbilityTask_ObserveElapsedTime.h"

URSAbilityTask_ObserveElapsedTime::URSAbilityTask_ObserveElapsedTime(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bTickingTask = true;
}

URSAbilityTask_ObserveElapsedTime* URSAbilityTask_ObserveElapsedTime::ObserveElapsedTime(UGameplayAbility* OwningAbility, float Duration)
{
	URSAbilityTask_ObserveElapsedTime* Task = NewAbilityTask<URSAbilityTask_ObserveElapsedTime>(OwningAbility);
	Task->Duration = Duration;

	return Task;
}

void URSAbilityTask_ObserveElapsedTime::Activate()
{
	Super::Activate();

	if (!FMath::IsFinite(Duration) || Duration <= 0.0f)
	{
		EndTask();

		return;
	}

	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnElapsedTimeUpdated.Broadcast(0.0f);
	}
}

void URSAbilityTask_ObserveElapsedTime::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);

	if (bHasFinished || !FMath::IsFinite(DeltaTime) || DeltaTime < 0.0f)
	{
		return;
	}

	ElapsedTime = FMath::Min(ElapsedTime + DeltaTime, Duration);
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnElapsedTimeUpdated.Broadcast(ElapsedTime);
	}

	if (ElapsedTime < Duration)
	{
		return;
	}

	bHasFinished = true;
	if (IsActive())
	{
		EndTask();
	}
}
