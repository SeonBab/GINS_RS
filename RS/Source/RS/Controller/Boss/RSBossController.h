#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "RSBossController.generated.h"

class ARSBossCharacter;
class UBehaviorTree;

/** 보스 캐릭터를 빙의하고 지정된 Behavior Tree를 실행하는 Controller입니다 */
UCLASS()
class RS_API ARSBossController : public AAIController
{
	GENERATED_BODY()

public:
	ARSBossController();

protected:
	virtual void OnPossess(APawn* InPawn) override;

public:
	/** 현재 빙의한 보스 캐릭터를 반환합니다 */
	UFUNCTION(BlueprintPure, Category = "RS|Boss")
	ARSBossCharacter* GetBossCharacter() const;

protected:
	/** 빙의한 보스가 사용할 Behavior Tree입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Boss")
	TObjectPtr<UBehaviorTree> BehaviorTree;
};
