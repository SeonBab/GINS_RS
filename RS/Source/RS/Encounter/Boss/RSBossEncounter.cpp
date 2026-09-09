#include "RSBossEncounter.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "CoreGlobals.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "RSBossCharacter.h"
#include "RSBossController.h"
#include "RSHealthComponent.h"
#include "RSHealthSet.h"
#include "RSPlayerCameraComponent.h"
#include "RSPlayerCharacter.h"
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
	CleanupOutcomeEvaluation();
	UnbindAllParticipantDeathObservations();
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

	DOREPLIFETIME(ARSBossEncounter, EncounterResult);
	DOREPLIFETIME(ARSBossEncounter, EncounterState);
}

void ARSBossEncounter::RegisterParticipant(ARSPlayerState* Participant)
{
	if (!HasAuthority() || !IsValid(Participant) || EncounterState == ERSBossEncounterState::Finished)
	{
		return;
	}

	if (Participants.Contains(Participant))
	{
		return;
	}

	Participants.Add(Participant);
	OnParticipantAdded.Broadcast(this, Participant);

	if (EncounterState == ERSBossEncounterState::Inactive)
	{
		BeginPreparing();
		StartEncounter();
	}
	else if (EncounterState == ERSBossEncounterState::Active)
	{
		BindParticipantDeathObservation(Participant);

		if (ARSBossController* BossController = GetBossController())
		{
			BossController->StartEncounter(this);
		}
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

	if (!Participants.Contains(Participant))
	{
		return;
	}

	UnbindParticipantDeathObservation(Participant);
	Participants.Remove(Participant);

	OnParticipantRemoved.Broadcast(this, Participant);
	RequestParticipantCameraDeactivation(Participant);
	UnregisterParticipantBossSource(Participant);
	UnregisterParticipantEncounterSource(Participant);

	if (ARSBossController* BossController = GetBossController())
	{
		BossController->RefreshTargetActor();
	}
}

void ARSBossEncounter::BeginPreparing()
{
	if (!HasAuthority() || EncounterState != ERSBossEncounterState::Inactive)
	{
		return;
	}

	EncounterResult = ERSBossEncounterResult::None;
	CompletionRemainingTimeSeconds = 0.0f;
	EncounterState = ERSBossEncounterState::Preparing;
	CleanupOutcomeEvaluation();
}

void ARSBossEncounter::StartEncounter()
{
	if (!HasAuthority() || EncounterState != ERSBossEncounterState::Preparing)
	{
		return;
	}

	const ERSBossEncounterState OldState = EncounterState;

	EncounterResult = ERSBossEncounterResult::None;
	EncounterState = ERSBossEncounterState::Active;

	// 시작 이벤트에서 유효한 제한 시간과 Boss 전투 상태를 관찰하도록 내부 준비를 먼저 끝냅니다
	for (const TWeakObjectPtr<ARSPlayerState>& ParticipantReference : Participants)
	{
		BindParticipantDeathObservation(ParticipantReference.Get());
	}

	StartTimeLimit();
	NotifyControllerEncounterStarted();
	BroadcastEncounterTransitionEvents(OldState);
}

void ARSBossEncounter::RequestClearOutcome()
{
	RecordOutcomeCandidate(ERSBossEncounterResult::Clear);
}

void ARSBossEncounter::ResolveEncounter(ERSBossEncounterResult Result)
{
	const bool bIsValidResult = Result == ERSBossEncounterResult::Clear || Result == ERSBossEncounterResult::Failed;
	if (!HasAuthority() || EncounterState != ERSBossEncounterState::Active || !bIsValidResult)
	{
		return;
	}

	const ERSBossEncounterState OldState = EncounterState;
	const float FinishedRemainingTimeSeconds = GetRemainingTimeSeconds();

	// 외부 getter가 중간 조합을 보지 않도록 종료 Snapshot과 Result를 먼저 구성한 뒤 State를 커밋합니다
	EncounterResult = Result;
	CompletionRemainingTimeSeconds = FinishedRemainingTimeSeconds;
	EncounterState = ERSBossEncounterState::Finished;

	CleanupOutcomeEvaluation();
	UnbindAllParticipantDeathObservations();
	StopTimeLimit();
	NotifyBossEncounterEnded();

	// Result UI와 종료 시점 Timer가 같은 Snapshot을 관찰하도록 Source는 Reset/EndPlay까지 유지합니다
	BroadcastEncounterTransitionEvents(OldState);
}

void ARSBossEncounter::ResetEncounter()
{
	if (!HasAuthority())
	{
		return;
	}

	const ERSBossEncounterState OldState = EncounterState;

	// 외부 콜백이 초기화 전의 논리 상태를 관찰하지 않도록 먼저 최종 상태를 커밋합니다
	EncounterResult = ERSBossEncounterResult::None;
	bHasTimeLimitExpired = false;
	CompletionRemainingTimeSeconds = 0.0f;
	EncounterState = ERSBossEncounterState::Inactive;

	CleanupOutcomeEvaluation();
	UnbindAllParticipantDeathObservations();
	StopTimeLimit();

	if (OldState == ERSBossEncounterState::Active)
	{
		NotifyBossEncounterEnded();
	}

	// 참가자 알림 전에 이 전이에서 수명이 끝나는 Source와 Camera 연결을 모두 정리합니다
	const TArray<TWeakObjectPtr<ARSPlayerState>> RemovedParticipants = Participants;
	Participants.Reset();

	for (const TWeakObjectPtr<ARSPlayerState>& ParticipantReference : RemovedParticipants)
	{
		ARSPlayerState* Participant = ParticipantReference.Get();

		if (Participant)
		{
			RequestParticipantCameraDeactivation(Participant);
			UnregisterParticipantBossSource(Participant);
			UnregisterParticipantEncounterSource(Participant);
		}
	}

	for (const TWeakObjectPtr<ARSPlayerState>& ParticipantReference : RemovedParticipants)
	{
		if (ARSPlayerState* Participant = ParticipantReference.Get())
		{
			OnParticipantRemoved.Broadcast(this, Participant);
		}
	}

	BroadcastEncounterTransitionEvents(OldState);
}

float ARSBossEncounter::GetRemainingTimeSeconds() const
{
	if (EncounterState == ERSBossEncounterState::Finished)
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

void ARSBossEncounter::BroadcastEncounterTransitionEvents(ERSBossEncounterState OldState)
{
	if (OldState != ERSBossEncounterState::Active && EncounterState == ERSBossEncounterState::Active)
	{
		OnEncounterStarted.Broadcast(this);
	}

	if (OldState == ERSBossEncounterState::Active && EncounterState != ERSBossEncounterState::Active)
	{
		OnEncounterEnded.Broadcast(this);
	}

	if (EncounterState == ERSBossEncounterState::Finished && EncounterResult != ERSBossEncounterResult::None)
	{
		OnEncounterFinished.Broadcast(this, EncounterResult);
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

void ARSBossEncounter::NotifyBossEncounterEnded()
{
	if (BossCharacter)
	{
		// Encounter는 ASC나 이동 구현을 직접 조작하지 않고 Boss 측 전투 종료 계약만 호출합니다
		BossCharacter->EndEncounterCombat();
	}
}

void ARSBossEncounter::BindParticipantDeathObservation(ARSPlayerState* Participant)
{
	if (EncounterState != ERSBossEncounterState::Active || !IsValid(Participant))
	{
		return;
	}

	ARSPlayerCharacter* PlayerCharacter = Cast<ARSPlayerCharacter>(Participant->GetPawn());
	URSHealthComponent* HealthComponent = PlayerCharacter ? PlayerCharacter->GetHealthComponent() : nullptr;
	if (!IsValid(HealthComponent))
	{
		return;
	}

	if (TWeakObjectPtr<URSHealthComponent>* ExistingHealthComponentReference = ParticipantDeathHealthComponents.Find(Participant))
	{
		if (ExistingHealthComponentReference->Get() == HealthComponent)
		{
			return;
		}

		if (URSHealthComponent* ExistingHealthComponent = ExistingHealthComponentReference->Get())
		{
			ExistingHealthComponent->OnDeathStarted.RemoveDynamic(this, &ThisClass::HandleParticipantDeathStarted);
		}
	}

	HealthComponent->OnDeathStarted.AddUniqueDynamic(this, &ThisClass::HandleParticipantDeathStarted);
	ParticipantDeathHealthComponents.Add(Participant, HealthComponent);
}

void ARSBossEncounter::UnbindParticipantDeathObservation(ARSPlayerState* Participant)
{
	TWeakObjectPtr<URSHealthComponent> HealthComponentReference;
	if (!ParticipantDeathHealthComponents.RemoveAndCopyValue(Participant, HealthComponentReference))
	{
		return;
	}

	if (URSHealthComponent* HealthComponent = HealthComponentReference.Get())
	{
		HealthComponent->OnDeathStarted.RemoveDynamic(this, &ThisClass::HandleParticipantDeathStarted);
	}
}

void ARSBossEncounter::UnbindAllParticipantDeathObservations()
{
	for (const TPair<TWeakObjectPtr<ARSPlayerState>, TWeakObjectPtr<URSHealthComponent>>& Observation : ParticipantDeathHealthComponents)
	{
		if (URSHealthComponent* HealthComponent = Observation.Value.Get())
		{
			HealthComponent->OnDeathStarted.RemoveDynamic(this, &ThisClass::HandleParticipantDeathStarted);
		}
	}

	ParticipantDeathHealthComponents.Reset();
}

void ARSBossEncounter::RequestFailedOutcome()
{
	RecordOutcomeCandidate(ERSBossEncounterResult::Failed);
}

void ARSBossEncounter::RecordOutcomeCandidate(ERSBossEncounterResult CandidateResult)
{
	if (!HasAuthority() || EncounterState != ERSBossEncounterState::Active)
	{
		return;
	}

	TOptional<uint64>* CandidateFrame = nullptr;
	if (CandidateResult == ERSBossEncounterResult::Clear)
	{
		CandidateFrame = &ClearCandidateFrame;
	}
	else if (CandidateResult == ERSBossEncounterResult::Failed)
	{
		CandidateFrame = &FailedCandidateFrame;
	}

	if (!CandidateFrame)
	{
		return;
	}

	if (!CandidateFrame->IsSet())
	{
		CandidateFrame->Emplace(GFrameCounter);
	}

	RequestOutcomeEvaluation();
}

void ARSBossEncounter::RequestOutcomeEvaluation()
{
	if (bIsOutcomeEvaluationPending || EncounterState != ERSBossEncounterState::Active)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	bIsOutcomeEvaluationPending = true;
	OutcomeEvaluationTimerHandle = World->GetTimerManager().SetTimerForNextTick(this, &ThisClass::EvaluateOutcome);
}

void ARSBossEncounter::EvaluateOutcome()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(OutcomeEvaluationTimerHandle);
	}

	bIsOutcomeEvaluationPending = false;
	OutcomeEvaluationTimerHandle.Invalidate();

	if (!HasAuthority() || EncounterState != ERSBossEncounterState::Active)
	{
		ClearCandidateFrame.Reset();
		FailedCandidateFrame.Reset();
		return;
	}

	const uint64 EarliestCandidateFrame = FMath::Min(
		ClearCandidateFrame.Get(MAX_uint64),
		FailedCandidateFrame.Get(MAX_uint64));

	// UE 5.8의 SetTimerForNextTick은 TimerManager가 아직 Tick하지 않았다면 같은 Frame에 실행될 수 있습니다
	// 최초 후보 Frame 전체를 취합한 뒤 판정하도록 엔진 Frame이 실제로 진행될 때까지 평가를 다시 예약합니다
	if (EarliestCandidateFrame != MAX_uint64 && GFrameCounter <= EarliestCandidateFrame)
	{
		RequestOutcomeEvaluation();
		return;
	}

	const ERSBossEncounterResult Result = SelectOutcomeResult(ClearCandidateFrame, FailedCandidateFrame);
	if (Result == ERSBossEncounterResult::None)
	{
		return;
	}

	ResolveEncounter(Result);
}

ERSBossEncounterResult ARSBossEncounter::SelectOutcomeResult(const TOptional<uint64>& ClearFrame, const TOptional<uint64>& FailedFrame)
{
	if (ClearFrame.IsSet() && (!FailedFrame.IsSet() || ClearFrame.GetValue() <= FailedFrame.GetValue()))
	{
		return ERSBossEncounterResult::Clear;
	}

	return FailedFrame.IsSet() ? ERSBossEncounterResult::Failed : ERSBossEncounterResult::None;
}

void ARSBossEncounter::CleanupOutcomeEvaluation()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(OutcomeEvaluationTimerHandle);
	}

	OutcomeEvaluationTimerHandle.Invalidate();
	bIsOutcomeEvaluationPending = false;
	ClearCandidateFrame.Reset();
	FailedCandidateFrame.Reset();
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
		// 카메라에는 Encounter가 아니라 공전 중심과 도메인 중립 조정값만 전달합니다
		PlayerCameraComp->ActivateBossCamera(CameraPivot, BossCameraSettings);
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

void ARSBossEncounter::HandleParticipantDeathStarted(URSHealthComponent* HealthComponent)
{
	if (EncounterState != ERSBossEncounterState::Active || !IsValid(HealthComponent))
	{
		return;
	}

	for (const TPair<TWeakObjectPtr<ARSPlayerState>, TWeakObjectPtr<URSHealthComponent>>& Observation : ParticipantDeathHealthComponents)
	{
		if (Observation.Value.Get() == HealthComponent)
		{
			RequestFailedOutcome();
			return;
		}
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
	BroadcastEncounterTransitionEvents(OldState);
}
