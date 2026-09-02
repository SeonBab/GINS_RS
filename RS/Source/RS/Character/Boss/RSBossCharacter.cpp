#include "RSBossCharacter.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "RSAbilitySystemComponent.h"
#include "RSAbilitySet.h"
#include "RSBossController.h"
#include "RSBossEncounter.h"
#include "RSHealthComponent.h"
#include "RSHealthSet.h"

ARSBossCharacter::ARSBossCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	AbilitySystemComp = CreateDefaultSubobject<URSAbilitySystemComponent>(TEXT("RSAbilitySystemComponent"));
	HealthSet = CreateDefaultSubobject<URSHealthSet>(TEXT("RSHealthSet"));
	HealthComp = CreateDefaultSubobject<URSHealthComponent>(TEXT("HealthComponent"));

	// 공격 판정이 진영을 콜리전 채널로 구분하므로 Blueprint 설정 누락을 막기 위해 캡슐 프로파일을 코드에서 고정합니다
	GetCapsuleComponent()->SetCollisionProfileName(TEXT("RSEnemyBody"));

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

void ARSBossCharacter::BeginPlay()
{
	Super::BeginPlay();

	InitializeAbilitySystem();
}

UAbilitySystemComponent* ARSBossCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComp;
}

void ARSBossCharacter::InitializeAbilitySystem()
{
	if (!AbilitySystemComp)
	{
		return;
	}

	AbilitySystemComp->InitAbilityActorInfo(this, this);

	// ActorInfo 초기화가 끝난 ASC와 HealthSet을 HealthComponent에 연결합니다
	HealthComp->InitializeWithAbilitySystem(AbilitySystemComp);

	if (AbilitySystemComp->IsOwnerActorAuthoritative() && !bDefaultAbilitiesGranted && DefaultAbilitySet)
	{
		DefaultAbilitySet->GiveToAbilitySystem(AbilitySystemComp, &GrantedAbilityHandles, this);

		bDefaultAbilitiesGranted = true;
	}
}

void ARSBossCharacter::SetBossEncounter(ARSBossEncounter* InBossEncounter)
{
	BossEncounter = InBossEncounter;
}
