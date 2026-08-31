#pragma once

#include "CoreMinimal.h"
#include "RSBaseCharacter.h"
#include "RSBossCharacter.generated.h"

/** 보스 전용 설정과 Controller 연결의 기반이 되는 캐릭터입니다 */
UCLASS()
class RS_API ARSBossCharacter : public ARSBaseCharacter
{
	GENERATED_BODY()

public:
	ARSBossCharacter();
};
