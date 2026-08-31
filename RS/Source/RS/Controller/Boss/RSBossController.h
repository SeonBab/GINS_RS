#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "TimerManager.h"
#include "RSBossController.generated.h"

class ARSBossCharacter;
class ARSBossEncounter;
class UBehaviorTree;

/** 보스 캐릭터를 빙의하고 지정된 Behavior Tree를 실행하는 Controller입니다 */
UCLASS()
class RS_API ARSBossController : public AAIController
{
	GENERATED_BODY()

public:
	/** 보스 Pawn과 함께 이동하며 Behavior Tree를 실행할 Controller 기본값을 구성합니다 */
	ARSBossController();

protected:
	/** 보스 캐릭터 빙의 후 Behavior Tree와 진행 중인 Encounter를 초기화합니다 */
	virtual void OnPossess(APawn* InPawn) override;

	/** 빙의 해제 전에 타깃 갱신 Timer와 Encounter 참조를 정리합니다 */
	virtual void OnUnPossess() override;

public:
	/** 현재 빙의한 보스 캐릭터를 반환합니다 */
	UFUNCTION(BlueprintPure, Category = "RS|Boss")
	ARSBossCharacter* GetBossCharacter() const;

	/** 보스전 참가자 목록을 사용하여 전투를 시작하고 공격 대상을 선택합니다 */
	void StartEncounter(ARSBossEncounter* InBossEncounter);

	/** 현재 공격 대상과 Blackboard 상태를 정리하고 보스전을 종료합니다 */
	void EndEncounter();

	/** 현재 대상이 유효하지 않으면 살아 있는 참가자 중 가장 가까운 Pawn을 선택합니다 */
	void RefreshTargetActor();

	/** 현재 공격 대상을 반환합니다 */
	UFUNCTION(BlueprintPure, Category = "RS|Boss")
	AActor* GetTargetActor() const { return TargetActor; }

private:
	/** 현재 공격 대상을 변경하고 Blackboard의 TargetActor 키를 함께 갱신합니다 */
	void SetTargetActor(AActor* InTargetActor);

protected:
	/** 빙의한 보스가 사용할 Behavior Tree입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Boss")
	TObjectPtr<UBehaviorTree> BehaviorTree;

	/** Blackboard에서 현재 공격 대상을 저장할 키 이름입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Boss")
	FName TargetActorKeyName = TEXT("TargetActor");

	/** 참가자의 사망이나 Pawn 교체를 반영하기 위해 공격 대상을 다시 확인하는 간격입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Boss", meta = (ClampMin = "0.1", Units = "s"))
	float TargetRefreshInterval = 0.5f;

private:
	/** 현재 진행 중인 보스전과 참가자 목록을 제공하는 Encounter입니다 */
	UPROPERTY(Transient)
	TObjectPtr<ARSBossEncounter> BossEncounter;

	/** Blackboard와 동기화되는 현재 공격 대상입니다 */
	UPROPERTY(Transient)
	TObjectPtr<AActor> TargetActor;

	/** 주기적인 공격 대상 검증을 중지하기 위한 Timer 핸들입니다 */
	FTimerHandle TargetRefreshTimerHandle;
};
