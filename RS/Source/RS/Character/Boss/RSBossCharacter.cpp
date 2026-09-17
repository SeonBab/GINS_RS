#include "RSBossCharacter.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInterface.h"
#include "RSAbilitySet.h"
#include "RSAbilitySystemComponent.h"
#include "RSAttackTelegraphComponent.h"
#include "RSBossController.h"
#include "RSBossEncounter.h"
#include "RSBossPhaseComponent.h"
#include "RSHealthComponent.h"
#include "RSHealthSet.h"
#include "RSHitFlashComponent.h"
#include "RSMusicPlaybackSubsystem.h"

ARSBossCharacter::ARSBossCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	AbilitySystemComp = CreateDefaultSubobject<URSAbilitySystemComponent>(TEXT("RSAbilitySystemComponent"));
	HealthSet = CreateDefaultSubobject<URSHealthSet>(TEXT("RSHealthSet"));
	HealthComp = CreateDefaultSubobject<URSHealthComponent>(TEXT("HealthComponent"));
	AttackTelegraphComp = CreateDefaultSubobject<URSAttackTelegraphComponent>(TEXT("AttackTelegraphComponent"));
	BossPhaseComp = CreateDefaultSubobject<URSBossPhaseComponent>(TEXT("BossPhaseComponent"));
	HitFlashComp = CreateDefaultSubobject<URSHitFlashComponent>(TEXT("HitFlashComponent"));

	// 공격 판정이 진영을 콜리전 채널로 구분하므로 Blueprint 설정 누락을 막기 위해 캡슐 프로파일을 코드에서 고정합니다
	GetCapsuleComponent()->SetCollisionProfileName(TEXT("RSEnemyBody"));

	// 바닥 예고 데칼의 투영 볼륨이 캐릭터에도 닿으므로 메시가 데칼을 받지 않게 합니다
	GetMesh()->SetReceivesDecals(false);

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

void ARSBossCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	HealthComp->OnDeathStarted.AddUniqueDynamic(this, &ThisClass::HandleDeathStarted);
	BossPhaseComp->OnPhaseEntered().AddUObject(this, &ThisClass::HandleBossPhaseEntered);
}

void ARSBossCharacter::BeginPlay()
{
	Super::BeginPlay();

	InitializeAbilitySystem();

	if (HitFlashComp)
	{
		HitFlashComp->Initialize(HealthComp, GetMesh());
	}
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

void ARSBossCharacter::HandleDeathStarted(URSHealthComponent* InHealthComponent)
{
	if (InHealthComponent != HealthComp)
	{
		return;
	}

	if (BossPhaseComp)
	{
		BossPhaseComp->RequestPersistentObjectCleanup();
	}

	// State.Dead는 새 Ability의 활성화만 막으므로 실행 중인 Ability는 직접 취소합니다
	if (AbilitySystemComp)
	{
		AbilitySystemComp->CancelAbilities();
	}

	if (ARSBossController* BossController = Cast<ARSBossController>(GetController()))
	{
		BossController->StopBossBehavior();
	}

	GetCharacterMovement()->DisableMovement();

	// 실행 중인 패턴 Montage를 멈추는 CancelAbilities() 뒤에 재생해야 사망 Montage가 함께 끊기지 않습니다
	PlayDeathMontage();

	// Player 사망과 같은 Frame에 발생할 수 있으므로 즉시 확정하지 않고 Clear 후보를 전달합니다
	if (BossEncounter)
	{
		BossEncounter->RequestClearOutcome();
	}
}

void ARSBossCharacter::HandleBossPhaseEntered(int32 PhaseIndex, USoundBase* PhaseMusic, float CrossfadeDuration)
{
	// 음악이 없는 페이즈에서도 외형은 갱신해야 하므로 음악 조기 반환보다 먼저 적용합니다
	ApplyPhaseMaterials(PhaseIndex);

	UWorld* World = GetWorld();
	if (!World || !PhaseMusic)
	{
		return;
	}

	if (URSMusicPlaybackSubsystem* MusicPlaybackSubsystem = World->GetSubsystem<URSMusicPlaybackSubsystem>())
	{
		MusicPlaybackSubsystem->PlayMusic(PhaseMusic, CrossfadeDuration);
	}
}

void ARSBossCharacter::ApplyPhaseMaterials(int32 PhaseIndex)
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
	{
		return;
	}

	for (const FRSBossPhaseMaterialOverride& MaterialOverride : PhaseMaterialOverrides)
	{
		if (MaterialOverride.PhaseIndex != PhaseIndex || !MaterialOverride.Material)
		{
			continue;
		}

		MeshComp->SetMaterial(MaterialOverride.MaterialSlotIndex, MaterialOverride.Material);
	}
}

void ARSBossCharacter::SetBossEncounter(ARSBossEncounter* InBossEncounter)
{
	BossEncounter = InBossEncounter;
}

void ARSBossCharacter::BeginEncounterCombat()
{
	if (BossPhaseComp)
	{
		// 페이즈 전환이 아니라 전투 시작에서만 첫 패턴을 늦추므로 페이즈 진입과 따로 요청합니다
		BossPhaseComp->BeginCombatStartGrace();
		BossPhaseComp->EnterCurrentPhase();
	}
}

void ARSBossCharacter::EndEncounterCombat()
{
	if (BossPhaseComp)
	{
		BossPhaseComp->RequestPersistentObjectCleanup();
	}

	// Failed에서도 보스는 죽지 않지만 이미 실행 중인 공격은 Encounter 수명과 함께 끝냅니다
	if (AbilitySystemComp)
	{
		AbilitySystemComp->CancelAbilities();
	}

	if (ARSBossController* BossController = Cast<ARSBossController>(GetController()))
	{
		BossController->EndEncounter();
	}
}
