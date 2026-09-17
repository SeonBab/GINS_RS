#pragma once

#if WITH_TESTS

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "RSScoreboardTestTypes.generated.h"

class URSScoreboardSubsystem;

/** 스코어보드 Cache 변경 동적 델리게이트를 수집하는 자동화 테스트 전용 객체입니다 */
UCLASS()
class URSScoreboardTestListener : public UObject
{
	GENERATED_BODY()

public:
	/** 지정한 Subsystem의 Cache 변경을 구독합니다 */
	void ListenTo(URSScoreboardSubsystem* ScoreboardSubsystem);

	/** 현재 구독 중인 Subsystem에서 안전하게 해제합니다 */
	void StopListening();

private:
	/** 확정 Cache 변경 횟수를 기록합니다 */
	UFUNCTION()
	void HandleScoreboardChanged();

public:
	/** 구독한 뒤 받은 Cache 변경 횟수입니다 */
	int32 ChangedCount = 0;

private:
	TWeakObjectPtr<URSScoreboardSubsystem> ObservedSubsystem;
};

#endif // WITH_TESTS
