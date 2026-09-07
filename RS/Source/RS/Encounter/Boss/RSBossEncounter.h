#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "RSBossEncounter.generated.h"

class APawn;
class ARSBossCharacter;
class ARSBossController;
class ARSBossEncounter;
class ARSPlayerController;
class ARSPlayerState;
class UBoxComponent;
class URSHealthComponent;
class URSPlayerCameraComponent;
class USceneComponent;

/** 보스전의 진행 상태입니다 */
UENUM(BlueprintType)
enum class ERSBossEncounterState : uint8
{
	/** 보스전이 시작되지 않았거나 초기화된 상태입니다 */
	Inactive,

	/** 참가자를 확정하고 실제 전투 시작에 필요한 준비를 수행하는 상태입니다 */
	Preparing,

	/** 한 명 이상의 참가자가 등록되어 보스전이 진행 중인 상태입니다 */
	Active,

	/** Clear 또는 Failed 결과가 확정되어 전투가 끝난 상태입니다 */
	Finished
};

/** 보스전이 끝난 원인입니다 */
UENUM(BlueprintType)
enum class ERSBossEncounterResult : uint8
{
	/** 보스전 결과가 아직 확정되지 않았습니다 */
	None,

	/** 보스를 처치하여 전투를 완료했습니다 */
	Clear,

	/** 플레이어 사망 등 실패 조건으로 전투가 끝났습니다 */
	Failed
};

/** 보스전의 시작과 종료를 외부 시스템에 전달하는 델리게이트입니다 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRSBossEncounterSignature, ARSBossEncounter*, BossEncounter);

/** 확정된 보스전 결과를 외부 시스템에 전달하는 델리게이트입니다 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRSBossEncounterFinishedSignature, ARSBossEncounter*, BossEncounter, ERSBossEncounterResult, Result);

/** 보스전 참가자의 등록과 제거를 외부 시스템에 전달하는 델리게이트입니다 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRSBossEncounterParticipantSignature, ARSBossEncounter*, BossEncounter, ARSPlayerState*, Participant);

/**
 * 보스 방에 진입한 플레이어를 전투 참가자로 유지하고 보스전 생명주기를 관리합니다
 * 시야와 관계없이 PlayerState를 기준으로 참가자를 보존하며 공격 대상 선택은 BossController에 위임합니다
 */
UCLASS()
class RS_API ARSBossEncounter : public AActor
{
	GENERATED_BODY()

public:
	/** 서버 권한으로 동작할 보스 방 영역과 복제 기본값을 구성합니다 */
	ARSBossEncounter();

protected:
	/** 서버에서 보스와 Encounter를 연결하고 이미 영역 안에 있는 플레이어를 등록합니다 */
	virtual void BeginPlay() override;

	/** Encounter가 제거되기 전에 예약된 제한 시간과 결과 평가, 참가자 구독과 데이터 원본 연결을 정리합니다 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 클라이언트가 전투의 진행 상태와 확정 결과를 알 수 있도록 State와 Result를 복제합니다 */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	/** 플레이어를 보스전 참가자로 등록하고 첫 참가자라면 전투를 시작합니다 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "RS|Boss")
	void RegisterParticipant(ARSPlayerState* Participant);

	/** 명시적으로 제외할 플레이어를 참가자 목록에서 제거합니다 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "RS|Boss")
	void UnregisterParticipant(ARSPlayerState* Participant);

	/** 최초 참가자 등록 이후 실제 전투 시작 전 준비 상태로 전환합니다 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "RS|Boss")
	void BeginPreparing();

	/** 준비가 끝난 Encounter의 제한 시간과 Boss 전투를 시작합니다 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "RS|Boss")
	void StartEncounter();

	/** Active Encounter의 Clear 또는 Failed 결과를 한 번만 확정합니다 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "RS|Boss")
	void ResolveEncounter(ERSBossEncounterResult Result);

	/** 보스 사망으로 발생한 Clear 후보를 현재 Frame의 결과 판정에 기록합니다 */
	void RequestClearOutcome();

	/** 보스전을 초기 상태로 되돌리고 기존 참가자를 모두 제거합니다 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "RS|Boss")
	void ResetEncounter();

	/** 현재 보스전이 진행 중인지 반환합니다 */
	UFUNCTION(BlueprintPure, Category = "RS|Boss")
	bool IsEncounterActive() const { return EncounterState == ERSBossEncounterState::Active; }

	/** 현재 보스전 상태를 반환합니다 */
	UFUNCTION(BlueprintPure, Category = "RS|Boss")
	ERSBossEncounterState GetEncounterState() const { return EncounterState; }

	/** 현재 확정된 보스전 결과를 반환합니다 */
	UFUNCTION(BlueprintPure, Category = "RS|Boss")
	ERSBossEncounterResult GetEncounterResult() const { return EncounterResult; }

	/**
	 * 표시할 수 있는 제한 시간의 남은 초를 반환하며 항상 0 이상입니다
	 * 완료된 전투는 처치 순간에 고정한 값을, 시작 전이나 만료 이후에는 0을 반환합니다
	 */
	UFUNCTION(BlueprintPure, Category = "RS|Boss")
	float GetRemainingTimeSeconds() const;

	/** 현재 보스전에서 제한 시간이 이미 만료되었는지 반환합니다 */
	UFUNCTION(BlueprintPure, Category = "RS|Boss")
	bool HasTimeLimitExpired() const { return bHasTimeLimitExpired; }

	/** 보스전 카메라가 궤도와 LookAt 계산에 사용할 공간 기준 Component를 반환합니다 */
	USceneComponent* GetCameraPivot() const;

	/** 현재 살아 있으며 공격 대상으로 사용할 수 있는 참가자 Pawn을 수집합니다 */
	void GetActiveParticipantPawns(TArray<APawn*>& OutParticipantPawns) const;

	/** Pawn이 현재 공격 가능한 참가자인지 확인합니다 */
	bool IsParticipantPawnActive(const APawn* ParticipantPawn) const;

	/**
	 * 제한 시간을 즉시 만료 상태로 만듭니다
	 * 자연 만료와 같은 최종 상태를 만드는 개발용 진입점이며 Blueprint에는 노출하지 않습니다
	 */
	void ForceExpireTimeLimit();

public:
	/** 첫 참가자가 등록되어 보스전이 시작될 때 발생합니다 */
	UPROPERTY(BlueprintAssignable, Category = "RS|Boss")
	FRSBossEncounterSignature OnEncounterStarted;

	/** Active 전투 구간의 내부 정리가 완료된 뒤 발생합니다 */
	UPROPERTY(BlueprintAssignable, Category = "RS|Boss")
	FRSBossEncounterSignature OnEncounterEnded;

	/** Clear 또는 Failed 결과와 내부 정리가 모두 확정된 뒤 발생합니다 */
	UPROPERTY(BlueprintAssignable, Category = "RS|Boss")
	FRSBossEncounterFinishedSignature OnEncounterFinished;

	/**
	 * 제한 시간이 만료될 때 한 번 발생하며 이후에도 보스전은 계속 진행됩니다
	 * 제한 시간은 복제하지 않으므로 서버에서만 발생합니다
	 */
	UPROPERTY(BlueprintAssignable, Category = "RS|Boss")
	FRSBossEncounterSignature OnTimeLimitExpired;

	/**
	 * 새로운 플레이어가 전투 참가자로 등록될 때 발생합니다
	 * 첫 참가자의 경우 아직 전투가 시작되기 전이므로 이 시점의 남은 시간은 0입니다
	 */
	UPROPERTY(BlueprintAssignable, Category = "RS|Boss")
	FRSBossEncounterParticipantSignature OnParticipantAdded;

	/** 플레이어가 전투 참가자 목록에서 명시적으로 제거될 때 발생합니다 */
	UPROPERTY(BlueprintAssignable, Category = "RS|Boss")
	FRSBossEncounterParticipantSignature OnParticipantRemoved;

protected:
	/** 참가자를 감지할 보스 방 영역입니다 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Boss")
	TObjectPtr<UBoxComponent> EncounterArea;

	/** BossCharacter의 이동과 애니메이션에 영향받지 않는 보스전 카메라 중심입니다 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Camera")
	TObjectPtr<USceneComponent> CameraPivot;

	/** 이 Encounter가 제어할 보스 캐릭터입니다 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "RS|Boss")
	TObjectPtr<ARSBossCharacter> BossCharacter;

private:
	/** 보스 방에 이미 존재하는 플레이어도 참가자로 등록합니다 */
	void RegisterOverlappingPlayers();

	/** BossCharacter를 빙의한 BossController를 반환하며 없으면 nullptr입니다 */
	ARSBossController* GetBossController() const;

	/** 내부 준비 또는 정리가 끝난 전이를 외부에 계약된 순서로 전달합니다 */
	void BroadcastEncounterTransitionEvents(ERSBossEncounterState OldState);

	/** BossController에 전투 시작을 알리고 첫 공격 대상을 선택하게 합니다 */
	void NotifyControllerEncounterStarted();

	/** BossController가 Blackboard와 현재 공격 대상을 정리하게 합니다 */
	void NotifyControllerEncounterEnded();

	/** Active 참가자의 현재 HealthComponent에 사망 관찰을 중복 없이 연결합니다 */
	void BindParticipantDeathObservation(ARSPlayerState* Participant);

	/** 참가자에 대해 실제 연결했던 HealthComponent에서 사망 관찰을 해제합니다 */
	void UnbindParticipantDeathObservation(ARSPlayerState* Participant);

	/** 모든 참가자 HealthComponent의 Active 범위 사망 관찰을 해제합니다 */
	void UnbindAllParticipantDeathObservations();

	/** 플레이어 사망으로 발생한 Failed 후보를 현재 Frame의 결과 판정에 기록합니다 */
	void RequestFailedOutcome();

	/** 결과 종류별 최초 후보 Frame을 기록하고 평가를 요청합니다 */
	void RecordOutcomeCandidate(ERSBossEncounterResult CandidateResult);

	/** 아직 예약되지 않았다면 다음 Tick에 결과 후보를 평가하도록 예약합니다 */
	void RequestOutcomeEvaluation();

	/** 가장 먼저 후보가 발생한 Frame을 기준으로 최종 결과를 선택합니다 */
	void EvaluateOutcome();

	/** 후보 Frame 조합에서 확정할 결과를 반환합니다 */
	static ERSBossEncounterResult SelectOutcomeResult(const TOptional<uint64>& ClearFrame, const TOptional<uint64>& FailedFrame);

	/** 예약된 평가와 결과 후보를 Active 수명 종료에 맞춰 정리합니다 */
	void CleanupOutcomeEvaluation();

	/** 만료 상태와 고정한 남은 시간을 초기화하고 제한 시간 타이머를 예약합니다 */
	void StartTimeLimit();

	/** 예약된 제한 시간 타이머만 제거하며 만료 상태와 고정한 남은 시간은 호출자가 관리합니다 */
	void StopTimeLimit();

	/** 제한 시간 만료를 기록하고 외부 시스템에 한 번만 전달합니다 */
	void HandleTimeLimitExpired();

	/**
	 * 참가자를 직접 조종하는 로컬 플레이어의 카메라 컴포넌트를 반환합니다
	 * 보스전 카메라는 로컬 표현이므로 이 프로세스에서 조종하지 않는 참가자는 대상이 아닙니다
	 */
	URSPlayerCameraComponent* GetParticipantCameraComponent(ARSPlayerState* Participant) const;

	/** 참가자의 로컬 카메라가 보스전 구도를 사용하도록 요청합니다 */
	void RequestParticipantCameraActivation(ARSPlayerState* Participant) const;

	/**
	 * 참가자의 로컬 카메라가 기본 플레이어 카메라로 돌아가도록 요청합니다
	 * 보스 처치 후에도 전투 구도를 유지하므로 참가자가 목록에서 빠질 때만 호출합니다
	 */
	void RequestParticipantCameraDeactivation(ARSPlayerState* Participant) const;

	/** 참가자를 조종하는 PlayerController를 반환하며 없으면 nullptr입니다 */
	ARSPlayerController* GetParticipantPlayerController(ARSPlayerState* Participant) const;

	/** 참가자의 로컬 ViewModel 저장소에 현재 보스를 데이터 원본으로 등록합니다 */
	void RegisterParticipantBossSource(ARSPlayerState* Participant) const;

	/** 참가자의 로컬 ViewModel 저장소에서 현재 보스 데이터 원본을 해제합니다 */
	void UnregisterParticipantBossSource(ARSPlayerState* Participant) const;

	/**
	 * 참가자의 로컬 ViewModel 저장소에 이 Encounter를 데이터 원본으로 등록합니다
	 * 제한 시간은 서버에서만 진행하므로 권한이 없는 경로에서는 등록하지 않습니다
	 */
	void RegisterParticipantEncounterSource(ARSPlayerState* Participant);

	/**
	 * 참가자의 로컬 ViewModel 저장소에서 이 Encounter 데이터 원본을 해제합니다
	 * 완료 후에도 결과 순간의 남은 시간을 계속 표시하므로 ResolveEncounter에서는 호출하지 않습니다
	 */
	void UnregisterParticipantEncounterSource(ARSPlayerState* Participant);

	/** 관찰 중인 참가자의 사망을 Failed 결과 후보로 변환합니다 */
	UFUNCTION()
	void HandleParticipantDeathStarted(URSHealthComponent* HealthComponent);

	/** 영역에 진입한 플레이어를 참가자로 등록합니다 */
	UFUNCTION()
	void HandleEncounterAreaBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/** 복제된 전투 상태 변경을 클라이언트 이벤트로 전달합니다 */
	UFUNCTION()
	void OnRep_EncounterState(ERSBossEncounterState OldState);

private:
	friend class FRSBossEncounterStateTest;

	/** 시야나 현재 Pawn의 변경과 관계없이 유지되는 전투 참가자 목록입니다 */
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<ARSPlayerState>> Participants;

	/** 참가자별로 실제 사망 이벤트를 연결한 HealthComponent입니다 */
	TMap<TWeakObjectPtr<ARSPlayerState>, TWeakObjectPtr<URSHealthComponent>> ParticipantDeathHealthComponents;

	/** 서버에서 결정하고 클라이언트에 복제하는 보스전 결과입니다 */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "RS|Boss", meta = (AllowPrivateAccess = "true"))
	ERSBossEncounterResult EncounterResult = ERSBossEncounterResult::None;

	/** 서버에서 결정하고 클라이언트에 복제하는 보스전 상태입니다 */
	UPROPERTY(ReplicatedUsing = OnRep_EncounterState, BlueprintReadOnly, Category = "RS|Boss", meta = (AllowPrivateAccess = "true"))
	ERSBossEncounterState EncounterState = ERSBossEncounterState::Inactive;

	/** 보스전이 시작된 뒤 허용하는 제한 시간이며 보스 방마다 다르게 설정합니다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RS|Boss", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0", Units = "s"))
	float TimeLimitSeconds = 300.0f;

	/** 제한 시간 만료를 예약한 타이머이며 남은 시간 조회의 기준입니다 */
	FTimerHandle TimeLimitTimerHandle;

	/** 현재 보스전에서 제한 시간이 이미 만료되었는지입니다 */
	bool bHasTimeLimitExpired = false;

	/** 전투가 완료된 순간에 고정한 남은 시간이며 완료 이후 표시에 사용합니다 */
	float CompletionRemainingTimeSeconds = 0.0f;

	/** Boss Death가 처음 기록된 엔진 Frame입니다 */
	TOptional<uint64> ClearCandidateFrame;

	/** Player Death가 처음 기록된 엔진 Frame입니다 */
	TOptional<uint64> FailedCandidateFrame;

	/** 다음 Tick의 결과 후보 평가가 이미 예약되어 있는지입니다 */
	bool bIsOutcomeEvaluationPending = false;

	/** 다음 Tick 결과 후보 평가를 취소하기 위한 Timer Handle입니다 */
	FTimerHandle OutcomeEvaluationTimerHandle;
};
