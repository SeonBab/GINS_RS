#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RSBossEncounter.generated.h"

class APawn;
class ARSBossCharacter;
class ARSBossEncounter;
class ARSPlayerState;
class UBoxComponent;

/** 보스전의 진행 상태입니다 */
UENUM(BlueprintType)
enum class ERSBossEncounterState : uint8
{
	/** 보스전이 시작되지 않았거나 초기화된 상태입니다 */
	Inactive,

	/** 한 명 이상의 참가자가 등록되어 보스전이 진행 중인 상태입니다 */
	Active,

	/** 보스 처치 등 정상적인 완료 조건을 충족한 상태입니다 */
	Completed
};

/** 보스전의 시작과 종료를 외부 시스템에 전달하는 델리게이트입니다 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRSBossEncounterSignature, ARSBossEncounter*, BossEncounter);

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

	/** 클라이언트가 전투 시작과 종료를 알 수 있도록 EncounterState를 복제합니다 */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	/** 플레이어를 보스전 참가자로 등록하고 첫 참가자라면 전투를 시작합니다 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "RS|Boss")
	void RegisterParticipant(ARSPlayerState* Participant);

	/** 명시적으로 제외할 플레이어를 참가자 목록에서 제거합니다 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "RS|Boss")
	void UnregisterParticipant(ARSPlayerState* Participant);

	/** 보스 처치 등 정상적인 종료 조건이 충족되었음을 기록합니다 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "RS|Boss")
	void CompleteEncounter();

	/** 보스전을 초기 상태로 되돌리고 기존 참가자를 모두 제거합니다 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "RS|Boss")
	void ResetEncounter();

	/** 현재 보스전이 진행 중인지 반환합니다 */
	UFUNCTION(BlueprintPure, Category = "RS|Boss")
	bool IsEncounterActive() const { return EncounterState == ERSBossEncounterState::Active; }

	/** 현재 보스전 상태를 반환합니다 */
	UFUNCTION(BlueprintPure, Category = "RS|Boss")
	ERSBossEncounterState GetEncounterState() const { return EncounterState; }

	/** 현재 살아 있으며 공격 대상으로 사용할 수 있는 참가자 Pawn을 수집합니다 */
	void GetActiveParticipantPawns(TArray<APawn*>& OutParticipantPawns) const;

	/** Pawn이 현재 공격 가능한 참가자인지 확인합니다 */
	bool IsParticipantPawnActive(const APawn* ParticipantPawn) const;

public:
	/** 첫 참가자가 등록되어 보스전이 시작될 때 발생합니다 */
	UPROPERTY(BlueprintAssignable, Category = "RS|Boss")
	FRSBossEncounterSignature OnEncounterStarted;

	/** 보스전이 완료되거나 초기화되어 종료될 때 발생합니다 */
	UPROPERTY(BlueprintAssignable, Category = "RS|Boss")
	FRSBossEncounterSignature OnEncounterEnded;

	/** 새로운 플레이어가 전투 참가자로 등록될 때 발생합니다 */
	UPROPERTY(BlueprintAssignable, Category = "RS|Boss")
	FRSBossEncounterParticipantSignature OnParticipantAdded;

	/** 플레이어가 전투 참가자 목록에서 명시적으로 제거될 때 발생합니다 */
	UPROPERTY(BlueprintAssignable, Category = "RS|Boss")
	FRSBossEncounterParticipantSignature OnParticipantRemoved;

protected:
	/** 참가자를 감지할 보스 방 영역입니다 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Boss")
	TObjectPtr<UBoxComponent> EncounterArea;

	/** 이 Encounter가 제어할 보스 캐릭터입니다 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "RS|Boss")
	TObjectPtr<ARSBossCharacter> BossCharacter;

private:
	/** 보스 방에 이미 존재하는 플레이어도 참가자로 등록합니다 */
	void RegisterOverlappingPlayers();

	/** 전투 상태를 변경하고 필요한 경우 서버의 대응 이벤트를 즉시 전달합니다 */
	void SetEncounterState(ERSBossEncounterState NewState, bool bBroadcastEvent = true);

	/** 상태 변경에 대응하는 전투 시작 또는 종료 이벤트를 전달합니다 */
	void BroadcastEncounterStateChanged(ERSBossEncounterState OldState);

	/** BossController에 전투 시작을 알리고 첫 공격 대상을 선택하게 합니다 */
	void NotifyControllerEncounterStarted();

	/** BossController가 Blackboard와 현재 공격 대상을 정리하게 합니다 */
	void NotifyControllerEncounterEnded();

	/** 영역에 진입한 플레이어를 참가자로 등록합니다 */
	UFUNCTION()
	void HandleEncounterAreaBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/** 복제된 전투 상태 변경을 클라이언트 이벤트로 전달합니다 */
	UFUNCTION()
	void OnRep_EncounterState(ERSBossEncounterState OldState);

private:
	/** 시야나 현재 Pawn의 변경과 관계없이 유지되는 전투 참가자 목록입니다 */
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<ARSPlayerState>> Participants;

	/** 서버에서 결정하고 클라이언트에 복제하는 보스전 상태입니다 */
	UPROPERTY(ReplicatedUsing = OnRep_EncounterState, BlueprintReadOnly, Category = "RS|Boss", meta = (AllowPrivateAccess = "true"))
	ERSBossEncounterState EncounterState = ERSBossEncounterState::Inactive;
};
