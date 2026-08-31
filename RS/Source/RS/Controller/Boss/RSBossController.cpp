#include "RSBossController.h"

#include "BehaviorTree/BehaviorTree.h"
#include "RSBossCharacter.h"

ARSBossController::ARSBossController()
{
	bAttachToPawn = true;
}

void ARSBossController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (!GetBossCharacter())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s can only run boss behavior for ARSBossCharacter"), *GetNameSafe(this));
		return;
	}

	if (!BehaviorTree)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s has no BehaviorTree"), *GetNameSafe(this));
		return;
	}

	if (!RunBehaviorTree(BehaviorTree))
	{
		UE_LOG(LogTemp, Error, TEXT("%s failed to run %s"), *GetNameSafe(this), *GetNameSafe(BehaviorTree));
	}
}

ARSBossCharacter* ARSBossController::GetBossCharacter() const
{
	return Cast<ARSBossCharacter>(GetPawn());
}
