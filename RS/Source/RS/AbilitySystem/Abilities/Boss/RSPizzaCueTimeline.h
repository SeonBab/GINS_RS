#pragma once

#include "CoreMinimal.h"

/** 예고 표시 시작부터 마지막 폭발까지의 실행 단계입니다 */
enum class ERSPizzaCueTimelinePhase : uint8
{
	Inactive,
	Cue,
	CueGap,
	RecallDelay,
	Explosion,
	ExplosionInterval,
	Complete
};

/** 단계별로 기다릴 공통 시간이며 각 패턴의 Definition이 자신의 설정으로 만듭니다 */
struct FRSPizzaCueTimings
{
	/** 한 단계의 위험 조각을 완성 상태로 표시해 둘 시간입니다 */
	float CueDuration = 0.0f;

	/** 마지막 단계를 제외한 예고 표시 사이에 아무것도 보여 주지 않을 시간입니다 */
	float CueGap = 0.0f;

	/** 마지막 예고를 지운 뒤 첫 폭발까지 플레이어가 이동할 시간입니다 */
	float RecallDelay = 0.0f;

	/** 첫 폭발 이후 다음 폭발까지의 공통 간격입니다 */
	float ExplosionInterval = 0.0f;
};

/**
 * 순서대로 예고한 뒤 같은 순서로 폭발시키는 피자 계열 패턴의 실행 상태입니다
 * Ability와 자동 테스트가 같은 단계 전이를 공유하며 시간값은 호출자가 넘깁니다
 */
struct FRSPizzaCueTimeline
{
	/** 유효한 단계 수로 첫 예고 표시 상태를 시작합니다 */
	bool Initialize(int32 InSequenceLength);

	/** 실행 상태를 비활성 초기값으로 되돌립니다 */
	void Reset();

	/** 현재 단계가 끝난 뒤 실행할 다음 단계로 이동합니다 */
	bool Advance();

	/** 현재 단계가 기다려야 하는 시간을 반환하며 대기가 없는 단계는 0입니다 */
	float GetCurrentDelaySeconds(const FRSPizzaCueTimings& Timings) const;

	/** 현재 실행 단계를 반환합니다 */
	ERSPizzaCueTimelinePhase GetPhase() const;

	/** 현재 예고 또는 폭발이 사용할 순서 인덱스를 반환합니다 */
	int32 GetSequenceIndex() const;

private:
	ERSPizzaCueTimelinePhase Phase = ERSPizzaCueTimelinePhase::Inactive;
	int32 SequenceLength = 0;
	int32 SequenceIndex = INDEX_NONE;
};
