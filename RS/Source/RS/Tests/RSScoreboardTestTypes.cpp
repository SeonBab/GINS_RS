#if WITH_TESTS

#include "RSScoreboardTestTypes.h"

#include "RSScoreboardSubsystem.h"

void URSScoreboardTestListener::ListenTo(URSScoreboardSubsystem* ScoreboardSubsystem)
{
	StopListening();
	if (!ScoreboardSubsystem)
	{
		return;
	}

	ObservedSubsystem = ScoreboardSubsystem;
	ScoreboardSubsystem->OnScoreboardChanged.AddDynamic(this, &ThisClass::HandleScoreboardChanged);
}

void URSScoreboardTestListener::StopListening()
{
	if (URSScoreboardSubsystem* ScoreboardSubsystem = ObservedSubsystem.Get())
	{
		ScoreboardSubsystem->OnScoreboardChanged.RemoveDynamic(this, &ThisClass::HandleScoreboardChanged);
	}

	ObservedSubsystem.Reset();
}

void URSScoreboardTestListener::HandleScoreboardChanged()
{
	++ChangedCount;
}

#endif // WITH_TESTS
