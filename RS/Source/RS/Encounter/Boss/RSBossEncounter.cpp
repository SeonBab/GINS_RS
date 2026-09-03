#include "RSBossEncounter.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "RSBossCharacter.h"
#include "RSBossController.h"
#include "RSHealthSet.h"
#include "RSPlayerCameraComponent.h"
#include "RSPlayerController.h"
#include "RSPlayerState.h"

ARSBossEncounter::ARSBossEncounter()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(false);

	EncounterArea = CreateDefaultSubobject<UBoxComponent>(TEXT("EncounterArea"));
	SetRootComponent(EncounterArea);

	EncounterArea->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	EncounterArea->SetCollisionObjectType(ECC_WorldDynamic);
	EncounterArea->SetCollisionResponseToAllChannels(ECR_Ignore);
	EncounterArea->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	EncounterArea->SetGenerateOverlapEvents(true);

	CameraPivot = CreateDefaultSubobject<USceneComponent>(TEXT("CameraPivot"));
	CameraPivot->SetupAttachment(EncounterArea);
}

void ARSBossEncounter::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		// 참가자 판정은 서버만 수행하고 클라이언트에는 EncounterState를 복제합니다
		return;
	}

	if (BossCharacter)
	{
		BossCharacter->SetBossEncounter(this);
	}

	// Encounter 내부에서 Pawn의 PlayerState 연결 전에 overlap이 시작되면 참가자 등록을 다시 시도할 이벤트가 없습니다
	// 현재는 PlayerState 초기화가 끝난 플레이어가 영역 밖에서 진입하는 레벨 흐름을 전제로 합니다
	EncounterArea->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleEncounterAreaBeginOverlap);
	RegisterOverlappingPlayers();
}

void ARSBossEncounter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (const TWeakObjectPtr<ARSPlayerState>& ParticipantReference : Participants)
	{
		UnregisterParticipantBossSource(ParticipantReference.Get());
	}

	Super::EndPlay(EndPlayReason);
}

void ARSBossEncounter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ARSBossEncounter, EncounterState);
}

void ARSBossEncounter::RegisterParticipant(ARSPlayerState* Participant)
{
	if (!HasAuthority() || !IsValid(Participant) || EncounterState == ERSBossEncounterState::Completed)
	{
		return;
	}

	const bool bIsAlreadyRegistered = Participants.ContainsByPredicate([Participant](const TWeakObjectPtr<ARSPlayerState>& ParticipantReference)
	{
		return ParticipantReference.Get() == Participant;
	});

	if (bIsAlreadyRegistered)
	{
		return;
	}

	Participants.Add(Participant);
	OnParticipantAdded.Broadcast(this, Participant);

	if (!IsEncounterActive())
	{
		const ERSBossEncounterState OldState = EncounterState;

		// 외부 시작 이벤트가 발생하기 전에 Controller와 Blackboard가 새 전투 상태를 사용하게 합니다
		SetEncounterState(ERSBossEncounterState::Active, false);
		NotifyControllerEncounterStarted();
		BroadcastEncounterStateChanged(OldState);
	}
	else if (ARSBossController* BossController = Cast<ARSBossController>(BossCharacter ? BossCharacter->GetController() : nullptr))
	{
		BossController->StartEncounter(this);
	}

	// 카메라 전환 기준은 전투 시작 순간이 아니라 이 플레이어가 참가자가 되었는지 여부입니다
	RequestParticipantCameraActivation(Participant);
	RegisterParticipantBossSource(Participant);
}

void ARSBossEncounter::UnregisterParticipant(ARSPlayerState* Participant)
{
	if (!HasAuthority() || !Participant)
	{
		return;
	}

	const int32 RemovedCount = Participants.RemoveAll([Participant](const TWeakObjectPtr<ARSPlayerState>& ParticipantReference)
	{
		return ParticipantReference.Get() == Participant;
	});

	if (RemovedCount == 0)
	{
		return;
	}

	OnParticipantRemoved.Broadcast(this, Participant);
	RequestParticipantCameraDeactivation(Participant);
	UnregisterParticipantBossSource(Participant);

	if (ARSBossController* BossController = Cast<ARSBossController>(BossCharacter ? BossCharacter->GetController() : nullptr))
	{
		BossController->RefreshTargetActor();
	}
}

void ARSBossEncounter::CompleteEncounter()
{
	if (!HasAuthority() || EncounterState == ERSBossEncounterState::Completed)
	{
		return;
	}

	NotifyControllerEncounterEnded();

	for (const TWeakObjectPtr<ARSPlayerState>& ParticipantReference : Participants)
	{
		UnregisterParticipantBossSource(ParticipantReference.Get());
	}

	SetEncounterState(ERSBossEncounterState::Completed);
}

void ARSBossEncounter::ResetEncounter()
{
	if (!HasAuthority())
	{
		return;
	}

	NotifyControllerEncounterEnded();

	// 목록을 먼저 비운 뒤 기존 참가자별 제거 이벤트를 전달하여 이벤트 수신자가 초기화된 상태를 조회하게 합니다
	const TArray<TWeakObjectPtr<ARSPlayerState>> RemovedParticipants = Participants;
	Participants.Reset();

	for (const TWeakObjectPtr<ARSPlayerState>& ParticipantReference : RemovedParticipants)
	{
		ARSPlayerState* Participant = ParticipantReference.Get();

		if (Participant)
		{
			OnParticipantRemoved.Broadcast(this, Participant);
			RequestParticipantCameraDeactivation(Participant);
			UnregisterParticipantBossSource(Participant);
		}
	}

	SetEncounterState(ERSBossEncounterState::Inactive);
}

void ARSBossEncounter::GetActiveParticipantPawns(TArray<APawn*>& OutParticipantPawns) const
{
	OutParticipantPawns.Reset();

	for (const TWeakObjectPtr<ARSPlayerState>& ParticipantReference : Participants)
	{
		const ARSPlayerState* Participant = ParticipantReference.Get();
		APawn* ParticipantPawn = Participant ? Participant->GetPawn() : nullptr;

		if (IsParticipantPawnActive(ParticipantPawn))
		{
			OutParticipantPawns.Add(ParticipantPawn);
		}
	}
}

USceneComponent* ARSBossEncounter::GetCameraPivot() const
{
	return CameraPivot;
}

bool ARSBossEncounter::IsParticipantPawnActive(const APawn* ParticipantPawn) const
{
	if (!IsValid(ParticipantPawn))
	{
		return false;
	}

	const ARSPlayerState* Participant = ParticipantPawn->GetPlayerState<ARSPlayerState>();
	const bool bIsRegistered = Participant && Participants.ContainsByPredicate([Participant](const TWeakObjectPtr<ARSPlayerState>& ParticipantReference)
	{
		return ParticipantReference.Get() == Participant;
	});

	if (!bIsRegistered)
	{
		return false;
	}

	const URSHealthSet* HealthSet = Participant->GetHealthSet();

	// 사망한 참가자는 목록에는 유지하지만 Controller의 현재 공격 대상 후보에서는 제외합니다
	return HealthSet && HealthSet->GetHealth() > 0.0f;
}

void ARSBossEncounter::RegisterOverlappingPlayers()
{
	TArray<AActor*> OverlappingActors;
	EncounterArea->GetOverlappingActors(OverlappingActors, APawn::StaticClass());

	for (AActor* OverlappingActor : OverlappingActors)
	{
		APawn* ParticipantPawn = Cast<APawn>(OverlappingActor);
		ARSPlayerState* Participant = ParticipantPawn ? ParticipantPawn->GetPlayerState<ARSPlayerState>() : nullptr;

		RegisterParticipant(Participant);
	}
}

void ARSBossEncounter::SetEncounterState(ERSBossEncounterState NewState, bool bBroadcastEvent)
{
	if (EncounterState == NewState)
	{
		return;
	}

	const ERSBossEncounterState OldState = EncounterState;
	EncounterState = NewState;

	if (bBroadcastEvent)
	{
		BroadcastEncounterStateChanged(OldState);
	}
}

void ARSBossEncounter::BroadcastEncounterStateChanged(ERSBossEncounterState OldState)
{
	if (OldState != ERSBossEncounterState::Active && EncounterState == ERSBossEncounterState::Active)
	{
		OnEncounterStarted.Broadcast(this);
	}
	else if (OldState == ERSBossEncounterState::Active && EncounterState != ERSBossEncounterState::Active)
	{
		OnEncounterEnded.Broadcast(this);
	}
}

void ARSBossEncounter::NotifyControllerEncounterStarted()
{
	if (!BossCharacter)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s has no BossCharacter"), *GetNameSafe(this));
		return;
	}

	ARSBossController* BossController = Cast<ARSBossController>(BossCharacter->GetController());
	if (!BossController)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s has no ARSBossController"), *GetNameSafe(BossCharacter));
		return;
	}

	BossController->StartEncounter(this);
}

void ARSBossEncounter::NotifyControllerEncounterEnded()
{
	ARSBossController* BossController = Cast<ARSBossController>(BossCharacter ? BossCharacter->GetController() : nullptr);

	if (BossController)
	{
		BossController->EndEncounter();
	}
}

URSPlayerCameraComponent* ARSBossEncounter::GetParticipantCameraComponent(ARSPlayerState* Participant) const
{
	if (!IsValid(Participant))
	{
		return nullptr;
	}

	ARSPlayerController* PlayerController = Cast<ARSPlayerController>(Participant->GetPlayerController());
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return nullptr;
	}

	return PlayerController->GetPlayerCameraComponent();
}

void ARSBossEncounter::RequestParticipantCameraActivation(ARSPlayerState* Participant) const
{
	if (URSPlayerCameraComponent* PlayerCameraComp = GetParticipantCameraComponent(Participant))
	{
		// 카메라에는 Encounter가 아니라 공전 중심이 될 Component만 전달합니다
		PlayerCameraComp->ActivateBossCamera(CameraPivot);
	}
}

void ARSBossEncounter::RequestParticipantCameraDeactivation(ARSPlayerState* Participant) const
{
	if (URSPlayerCameraComponent* PlayerCameraComp = GetParticipantCameraComponent(Participant))
	{
		PlayerCameraComp->DeactivateBossCamera();
	}
}

void ARSBossEncounter::RegisterParticipantBossSource(ARSPlayerState* Participant) const
{
	ARSPlayerController* PlayerController = IsValid(Participant) ? Cast<ARSPlayerController>(Participant->GetPlayerController()) : nullptr;

	if (PlayerController && BossCharacter)
	{
		PlayerController->RegisterViewModelSource(BossCharacter);
	}
}

void ARSBossEncounter::UnregisterParticipantBossSource(ARSPlayerState* Participant) const
{
	ARSPlayerController* PlayerController = IsValid(Participant) ? Cast<ARSPlayerController>(Participant->GetPlayerController()) : nullptr;

	if (PlayerController)
	{
		PlayerController->UnregisterViewModelSource(BossCharacter);
	}
}

void ARSBossEncounter::HandleEncounterAreaBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	APawn* ParticipantPawn = Cast<APawn>(OtherActor);
	ARSPlayerState* Participant = ParticipantPawn ? ParticipantPawn->GetPlayerState<ARSPlayerState>() : nullptr;

	RegisterParticipant(Participant);
}

void ARSBossEncounter::OnRep_EncounterState(ERSBossEncounterState OldState)
{
	BroadcastEncounterStateChanged(OldState);
}
