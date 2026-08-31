#pragma once

#include "CoreMinimal.h"
#include "RSBaseCharacter.h"
#include "RSBossCharacter.generated.h"

class ARSBossEncounter;

/** 보스 전용 설정과 Controller 연결의 기반이 되는 캐릭터입니다 */
UCLASS()
class RS_API ARSBossCharacter : public ARSBaseCharacter
{
	GENERATED_BODY()

public:
	/** 보스가 배치되거나 생성될 때 BossController가 자동으로 빙의하도록 기본값을 구성합니다 */
	ARSBossCharacter();

	/** 이 캐릭터의 전투 참가자와 생명주기를 관리할 Encounter를 설정합니다 */
	void SetBossEncounter(ARSBossEncounter* InBossEncounter);

	/** 이 캐릭터에 연결된 Encounter를 반환합니다 */
	ARSBossEncounter* GetBossEncounter() const { return BossEncounter; }

private:
	/** 보스전 상태와 참가자 목록을 소유하는 Encounter입니다 */
	UPROPERTY(Transient)
	TObjectPtr<ARSBossEncounter> BossEncounter;
};
