#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RSBossPersistentObjectLifetimeComponent.generated.h"

class URSBossPhaseComponent;

/** 보스의 패턴 진행에 따라 지속 오브젝트가 정리되어야 함을 소유 Actor에 알립니다 */
DECLARE_MULTICAST_DELEGATE(FRSBossPersistentObjectCleanupRequiredSignature);

/**
 * 보스 Phase Component의 패턴 수명 신호를 개별 지속 오브젝트의 정리 요청으로 변환합니다
 * 오브젝트의 상태와 제거 방식은 알지 않으며 Phase 구독, 생성 순번과 offset 판정만 소유합니다
 */
UCLASS(ClassGroup = "RS")
class RS_API URSBossPersistentObjectLifetimeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URSBossPersistentObjectLifetimeComponent();

protected:
	/** 소유 Actor가 제거될 때 Phase 이벤트 구독을 해제합니다 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	/** 보스의 현재 특수 패턴 순번을 캡처하고 이후 수명 이벤트 구독을 시작합니다 */
	bool StartObserving(AActor* BossActor, int32 InSpecialPatternCleanupOffset);

	/** 현재 Phase 이벤트 구독을 해제하고 캡처한 수명 상태를 초기화합니다 */
	void StopObserving();

	/** 생성 순번과 offset을 기준으로 현재 특수 패턴 요청에서 정리할지 반환합니다 */
	static bool ShouldCleanupForSpecialPattern(int32 SpawnSequence, int32 ActiveSequence, int32 SpecialPatternCleanupOffset);

	/** 지속 오브젝트의 정리가 필요할 때 발행하는 이벤트를 반환합니다 */
	FRSBossPersistentObjectCleanupRequiredSignature& OnCleanupRequired() { return CleanupRequiredEvent; }

	/** 유효한 Phase Component를 구독 중인지 반환합니다 */
	bool IsObserving() const { return BossPhaseComp.IsValid(); }

	/** 구독 시작 시 캡처한 특수 패턴 순번을 반환합니다 */
	int32 GetSpawnSpecialPatternSequence() const { return SpawnSpecialPatternSequence; }

private:
	/** 구독을 먼저 끊고 정리 요청을 한 번만 발행합니다 */
	void RequestCleanup();

	/** 설정된 offset에 도달한 특수 패턴 요청을 정리 요청으로 변환합니다 */
	void HandleSpecialPatternActivationRequested(int32 ActiveSequence);

	/** 메인 기믹, 보스 사망과 Encounter 종료의 공용 신호를 정리 요청으로 전달합니다 */
	void HandlePersistentObjectCleanupRequested();

private:
	TWeakObjectPtr<URSBossPhaseComponent> BossPhaseComp;
	FDelegateHandle SpecialPatternActivationRequestedDelegateHandle;
	FDelegateHandle PersistentObjectCleanupDelegateHandle;
	FRSBossPersistentObjectCleanupRequiredSignature CleanupRequiredEvent;
	int32 SpawnSpecialPatternSequence = 0;
	int32 SpecialPatternCleanupOffset = 0;
};
