#include "RSBossCharacter.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "RSBossController.h"
#include "RSBossEncounter.h"

ARSBossCharacter::ARSBossCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	// 레벨 배치와 런타임 생성 방식이 달라도 동일한 Controller 초기화 흐름을 사용합니다
	AIControllerClass = ARSBossController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Behavior Tree의 Focus 방향을 따라가되 RotationRate로 보스의 회전 속도를 제한합니다
	UCharacterMovementComponent* CharacterMovementComp = GetCharacterMovement();
	CharacterMovementComp->bOrientRotationToMovement = false;
	CharacterMovementComp->bUseControllerDesiredRotation = true;
	CharacterMovementComp->RotationRate = FRotator(0.0f, 240.0f, 0.0f);
}

void ARSBossCharacter::SetBossEncounter(ARSBossEncounter* InBossEncounter)
{
	BossEncounter = InBossEncounter;
}
