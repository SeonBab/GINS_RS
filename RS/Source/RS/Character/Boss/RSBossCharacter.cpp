#include "RSBossCharacter.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "RSBossController.h"

ARSBossCharacter::ARSBossCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	AIControllerClass = ARSBossController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
}
