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

namespace
{
	/** FTimerManager는 0 이하의 시간에 타이머를 예약하지 않으므로 만료가 발생할 수 있는 최소 시간을 보장합니다 */
	constexpr float MinimumTimeLimitSeconds = 1.0f;
}

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
	StopTimeLimit();

	for (const TWeakObjectPtr<ARSPlayerState>& ParticipantReference : Participants)
	{
		UnregisterParticipantBossSource(ParticipantReference.Get());
		UnregisterParticipantEncounterSource(ParticipantReference.Get());
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

	if (Participants.Contains(Participant))
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

		// 전투 시작을 외부에서 관찰할 수 있게 되기 전에 남은 시간이 유효해지도록 먼저 예약합니다
		StartTimeLimit();
		NotifyControllerEncounterStarted();
		BroadcastEncounterStateChanged(OldState);
	}
	else if (ARSBossController* BossController = GetBossController())
	{
		BossController->StartEncounter(this);
	}

	// 카메라 전환 기준은 전투 시작 순간이 아니라 이 플레이어가 참가자가 되었는지 여부입니다
	RequestParticipantCameraActivation(Participant);
	RegisterParticipantBossSource(Participant);
	RegisterParticipantEncounterSource(Participant);
}

void ARSBossEncounter::UnregisterParticipant(ARSPlayerState* Participant)
{
	if (!HasAuthority() || !Participant)
	{
		return;
	}

	if (Participants.Remove(Participant) == 0)
	{
		return;
	}

	OnParticipantRemoved.Broadcast(this, Participant);
	RequestParticipantCameraDeactivation(Participant);
	UnregisterParticipantBossSource(Participant);
	UnregisterParticipantEncounterSource(Participant);

	if (ARSBossController* BossController = GetBossController())
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

	// 상태를 공개하기 전에 남은 시간을 고정하고 예약을 제거하여, 종료 이벤트를 받은 쪽이 확정된 값을 읽게 합니다
	CompletionRemainingTimeSeconds = GetRemainingTimeSeconds();
	StopTimeLimit();

	NotifyControllerEncounterEnded();

	// 완료 후에도 처치 순간의 남은 시간을 계속 표시하므로 Encounter 데이터 원본은 해제하지 않습니다
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

	// 초기화된 전투에서 예약된 만료 콜백이 뒤늦게 실행되지 않도록 제한 시간 상태를 먼저 되돌립니다
	StopTimeLimit();
	bHasTimeLimitExpired = false;
	CompletionRemainingTimeSeconds = 0.0f;

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
			UnregisterParticipantEncounterSource(Participant);
		}
	}

	SetEncounterState(ERSBossEncounterState::Inactive);
}

float ARSBossEncounter::GetRemainingTimeSeconds() const
{
	if (EncounterState == ERSBossEncounterState::Completed)
	{
		return FMath::Max(CompletionRemainingTimeSeconds, 0.0f);
	}

	// AActor::GetWorldTimerManager는 월드를 확인하지 않으므로 매 프레임 조회되는 이 경로에서는 사용하지 않습니다
	const UWorld* World = GetWorld();
	const float RemainingSeconds = World ? World->GetTimerManager().GetTimerRemaining(TimeLimitTimerHandle) : -1.0f;

	// 예약되지 않았거나 이미 만료된 타이머는 -1을 반환하므로 표시 계층에 그대로 전달하지 않습니다
	return FMath::Max(RemainingSeconds, 0.0f);
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
	if (!Participant || !Participants.Contains(Participant))
	{
		return false;
	}

	const URSHealthSet* HealthSet = Participant->GetHealthSet();

	// 사망한 참가자는 목록에는 유지하지만 Controller의 현재 공격 대상 후보에서는 제외합니다
	return HealthSet && HealthSet->GetHealth() > 0.0f;
}

void ARSBossEncounter::ForceExpireTimeLimit()
{
	// 예약을 먼저 제거하여 강제 만료와 자연 만료가 겹쳐 두 번 전달되지 않게 합니다
	StopTimeLimit();
	HandleTimeLimitExpired();
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

ARSBossController* ARSBossEncounter::GetBossController() const
{
	return Cast<ARSBossController>(BossCharacter ? BossCharacter->GetController() : nullptr);
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
	ARSBossController* BossController = GetBossController();

	if (BossController)
	{
		BossController->EndEncounter();
	}
}

void ARSBossEncounter::StartTimeLimit()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 이전 전투의 결과가 남지 않도록 이 함수 하나로 진행 중인 제한 시간의 상태를 모두 확정합니다
	bHasTimeLimitExpired = false;
	CompletionRemainingTimeSeconds = 0.0f;

	if (TimeLimitSeconds < MinimumTimeLimitSeconds)
	{
		// 만료가 영원히 오지 않는 상태를 조용히 만들지 않도록 잘못된 설정값을 알립니다
		UE_LOG(LogTemp, Warning, TEXT("%s has invalid TimeLimitSeconds %.2f"), *GetNameSafe(this), TimeLimitSeconds);
	}

	const float SafeTimeLimitSeconds = FMath::Max(TimeLimitSeconds, MinimumTimeLimitSeconds);
	World->GetTimerManager().SetTimer(TimeLimitTimerHandle, this, &ThisClass::HandleTimeLimitExpired, SafeTimeLimitSeconds, false);
}

void ARSBossEncounter::StopTimeLimit()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimeLimitTimerHandle);
	}

	TimeLimitTimerHandle.Invalidate();
}

void ARSBossEncounter::HandleTimeLimitExpired()
{
	// 강제 만료로 진행 중이 아닌 전투에 도달할 수 있으므로 상태를 다시 확인합니다
	if (EncounterState != ERSBossEncounterState::Active || bHasTimeLimitExpired)
	{
		return;
	}

	// 만료를 전달받은 쪽이 확정된 상태를 조회하도록 핸들과 상태를 먼저 정리합니다
	TimeLimitTimerHandle.Invalidate();
	bHasTimeLimitExpired = true;

	OnTimeLimitExpired.Broadcast(this);
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

ARSPlayerController* ARSBossEncounter::GetParticipantPlayerController(ARSPlayerState* Participant) const
{
	return IsValid(Participant) ? Cast<ARSPlayerController>(Participant->GetPlayerController()) : nullptr;
}

void ARSBossEncounter::RegisterParticipantBossSource(ARSPlayerState* Participant) const
{
	ARSPlayerController* PlayerController = GetParticipantPlayerController(Participant);

	if (PlayerController && BossCharacter)
	{
		PlayerController->RegisterViewModelSource(BossCharacter);
	}
}

void ARSBossEncounter::UnregisterParticipantBossSource(ARSPlayerState* Participant) const
{
	ARSPlayerController* PlayerController = GetParticipantPlayerController(Participant);

	if (PlayerController)
	{
		PlayerController->UnregisterViewModelSource(BossCharacter);
	}
}

void ARSBossEncounter::RegisterParticipantEncounterSource(ARSPlayerState* Participant)
{
	ARSPlayerController* PlayerController = GetParticipantPlayerController(Participant);

	if (PlayerController)
	{
		PlayerController->RegisterViewModelSource(this);
	}
}

void ARSBossEncounter::UnregisterParticipantEncounterSource(ARSPlayerState* Participant)
{
	ARSPlayerController* PlayerController = GetParticipantPlayerController(Participant);

	if (PlayerController)
	{
		PlayerController->UnregisterViewModelSource(this);
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
