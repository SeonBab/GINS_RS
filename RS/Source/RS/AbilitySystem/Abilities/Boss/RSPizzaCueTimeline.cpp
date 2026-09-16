#include "RSPizzaCueTimeline.h"

bool FRSPizzaCueTimeline::Initialize(int32 InSequenceLength)
{
	Reset();
	if (InSequenceLength <= 0)
	{
		return false;
	}

	SequenceLength = InSequenceLength;
	SequenceIndex = 0;
	Phase = ERSPizzaCueTimelinePhase::Cue;

	return true;
}

void FRSPizzaCueTimeline::Reset()
{
	Phase = ERSPizzaCueTimelinePhase::Inactive;
	SequenceLength = 0;
	SequenceIndex = INDEX_NONE;
}

bool FRSPizzaCueTimeline::Advance()
{
	switch (Phase)
	{
	case ERSPizzaCueTimelinePhase::Cue:
		Phase = SequenceIndex + 1 < SequenceLength
			? ERSPizzaCueTimelinePhase::CueGap
			: ERSPizzaCueTimelinePhase::RecallDelay;
		return true;

	case ERSPizzaCueTimelinePhase::CueGap:
		++SequenceIndex;
		Phase = ERSPizzaCueTimelinePhase::Cue;
		return SequenceIndex < SequenceLength;

	case ERSPizzaCueTimelinePhase::RecallDelay:
		SequenceIndex = 0;
		Phase = ERSPizzaCueTimelinePhase::Explosion;
		return true;

	case ERSPizzaCueTimelinePhase::Explosion:
		Phase = SequenceIndex + 1 < SequenceLength
			? ERSPizzaCueTimelinePhase::ExplosionInterval
			: ERSPizzaCueTimelinePhase::Complete;
		return true;

	case ERSPizzaCueTimelinePhase::ExplosionInterval:
		++SequenceIndex;
		Phase = ERSPizzaCueTimelinePhase::Explosion;
		return SequenceIndex < SequenceLength;

	default:
		return false;
	}
}

float FRSPizzaCueTimeline::GetCurrentDelaySeconds(const FRSPizzaCueTimings& Timings) const
{
	switch (Phase)
	{
	case ERSPizzaCueTimelinePhase::Cue:
		return Timings.CueDuration;

	case ERSPizzaCueTimelinePhase::CueGap:
		return Timings.CueGap;

	case ERSPizzaCueTimelinePhase::RecallDelay:
		return Timings.RecallDelay;

	case ERSPizzaCueTimelinePhase::ExplosionInterval:
		return Timings.ExplosionInterval;

	default:
		return 0.0f;
	}
}

ERSPizzaCueTimelinePhase FRSPizzaCueTimeline::GetPhase() const
{
	return Phase;
}

int32 FRSPizzaCueTimeline::GetSequenceIndex() const
{
	return SequenceIndex;
}
