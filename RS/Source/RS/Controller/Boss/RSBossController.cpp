#include "RSBossController.h"

#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BrainComponent.h"
#include "GameFramework/Pawn.h"
#include "RSBossCharacter.h"
#include "RSBossEncounter.h"
#include "TimerManager.h"

ARSBossController::ARSBossController()
{
	bAttachToPawn = true;
}

void ARSBossController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	ARSBossCharacter* PossessedBossCharacter = GetBossCharacter();
	if (!PossessedBossCharacter)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s can only run boss behavior for ARSBossCharacter"), *GetNameSafe(this));
		return;
	}

	if (!BehaviorTree)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s has no BehaviorTree"), *GetNameSafe(this));
		return;
	}

	// RunBehaviorTree가 Blackboard를 초기화한 뒤 Encounter의 TargetActor를 동기화합니다
	if (!RunBehaviorTree(BehaviorTree))
	{
		UE_LOG(LogTemp, Error, TEXT("%s failed to run %s"), *GetNameSafe(this), *GetNameSafe(BehaviorTree));
		return;
	}

	ARSBossEncounter* PossessedBossEncounter = PossessedBossCharacter->GetBossEncounter();
	if (PossessedBossEncounter && PossessedBossEncounter->IsEncounterActive())
	{
		StartEncounter(PossessedBossEncounter);
	}
}

void ARSBossController::OnUnPossess()
{
	EndEncounter();

	Super::OnUnPossess();
}

ARSBossCharacter* ARSBossController::GetBossCharacter() const
{
	return Cast<ARSBossCharacter>(GetPawn());
}

void ARSBossController::StartEncounter(ARSBossEncounter* InBossEncounter)
{
	if (!InBossEncounter || !InBossEncounter->IsEncounterActive())
	{
		return;
	}

	BossEncounter = InBossEncounter;
	RefreshTargetActor();

	// PlayerState가 새 Pawn을 소유하거나 현재 타깃이 사망한 상황을 별도 감지 시스템 없이 반영합니다
	GetWorldTimerManager().SetTimer(TargetRefreshTimerHandle, this, &ThisClass::RefreshTargetActor, TargetRefreshInterval, true);
}

void ARSBossController::EndEncounter()
{
	GetWorldTimerManager().ClearTimer(TargetRefreshTimerHandle);

	SetTargetActor(nullptr);
	BossEncounter = nullptr;
}

void ARSBossController::StopBossBehavior()
{
	// 중단되는 Task가 이동과 Focus를 다시 설정하지 않도록 Behavior Tree를 먼저 중지합니다
	if (BrainComponent)
	{
		BrainComponent->StopLogic(TEXT("Boss death"));
	}

	StopMovement();
	ClearFocus(EAIFocusPriority::Gameplay);

	EndEncounter();
}

void ARSBossController::RefreshTargetActor()
{
	if (!BossEncounter || !BossEncounter->IsEncounterActive())
	{
		SetTargetActor(nullptr);
		return;
	}

	const APawn* CurrentTargetPawn = Cast<APawn>(TargetActor);
	if (BossEncounter->IsParticipantPawnActive(CurrentTargetPawn))
	{
		// 매 갱신마다 가까운 대상으로 바뀌는 현상을 막기 위해 유효한 현재 타깃을 우선 유지합니다
		return;
	}

	const APawn* BossPawn = GetPawn();
	if (!BossPawn)
	{
		SetTargetActor(nullptr);
		return;
	}

	TArray<APawn*> ParticipantPawns;
	BossEncounter->GetActiveParticipantPawns(ParticipantPawns);

	APawn* NearestParticipantPawn = nullptr;
	float NearestDistanceSquared = TNumericLimits<float>::Max();

	// 현재 타깃을 사용할 수 없을 때만 가장 가까운 생존 참가자를 대체 대상으로 선택합니다
	for (APawn* ParticipantPawn : ParticipantPawns)
	{
		const float DistanceSquared = FVector::DistSquared(BossPawn->GetActorLocation(), ParticipantPawn->GetActorLocation());

		if (DistanceSquared < NearestDistanceSquared)
		{
			NearestParticipantPawn = ParticipantPawn;
			NearestDistanceSquared = DistanceSquared;
		}
	}

	SetTargetActor(NearestParticipantPawn);
}

void ARSBossController::SetTargetActor(AActor* InTargetActor)
{
	if (TargetActor == InTargetActor)
	{
		return;
	}

	TargetActor = InTargetActor;

	// Behavior Tree가 같은 대상을 사용하도록 Controller 상태와 Blackboard 값을 함께 변경합니다
	UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
	if (!BlackboardComp)
	{
		return;
	}

	BlackboardComp->SetValueAsObject(TargetActorKeyName, TargetActor);
}
