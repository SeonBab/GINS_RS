#include "RSAbilityTask_WaitTelegraphFill.h"

URSAbilityTask_WaitTelegraphFill::URSAbilityTask_WaitTelegraphFill(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bTickingTask = true;
}

URSAbilityTask_WaitTelegraphFill* URSAbilityTask_WaitTelegraphFill::WaitTelegraphFill(UGameplayAbility* OwningAbility, float Duration)
{
	URSAbilityTask_WaitTelegraphFill* Task = NewAbilityTask<URSAbilityTask_WaitTelegraphFill>(OwningAbility);
	Task->Duration = Duration;

	return Task;
}

void URSAbilityTask_WaitTelegraphFill::Activate()
{
	Super::Activate();

	if (!FMath::IsFinite(Duration) || Duration <= 0.0f)
	{
		EndTask();

		return;
	}

	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnProgress.Broadcast(0.0f);
	}
}

void URSAbilityTask_WaitTelegraphFill::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);

	if (bHasFinished || !FMath::IsFinite(DeltaTime) || DeltaTime < 0.0f)
	{
		return;
	}

	ElapsedTime = FMath::Min(ElapsedTime + DeltaTime, Duration);
	const float Fill = ElapsedTime / Duration;
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnProgress.Broadcast(Fill);
	}

	if (ElapsedTime < Duration)
	{
		return;
	}

	bHasFinished = true;
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnFinished.Broadcast();
	}

	if (IsActive())
	{
		EndTask();
	}
}
