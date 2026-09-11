#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "RSBossMeteorHazard.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class USceneComponent;
class URSAttackTelegraphComponent;
class URSBossPersistentObjectLifetimeComponent;
#if WITH_EDITOR
class FDataValidationContext;
#endif

/** 운석 예고와 장판이 현재 어느 단계인지 나타냅니다 */
UENUM(BlueprintType)
enum class ERSBossMeteorHazardState : uint8
{
	Tracking,
	Locked,
	Hazard,
	Cleanup
};

/** 운석 예고가 장판 전환과 조기 정리 중 어떤 결과로 끝났는지 나타냅니다 */
enum class ERSBossMeteorHazardPreparationResult : uint8
{
	HazardActivated,
	CleanedUp
};

/** 운석 예고, 장판과 수명에 필요한 설정입니다 */
USTRUCT(BlueprintType)
struct FRSBossMeteorHazardDefinition
{
	GENERATED_BODY()

	/** Telegraph가 0에서 1까지 채워지는 전체 시간입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Meteor Hazard|Timing", meta = (ClampMin = "0.01", UIMin = "0.01", ForceUnits = "s"))
	float TelegraphDuration = 3.0f;

	/** 낙하 Niagara가 장판 활성화보다 먼저 시작하는 시간입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Meteor Hazard|Timing", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "s"))
	float FallEffectLeadTime = 1.0f;

	/** Telegraph 표시와 수평 HitCheck가 공유하는 최종 반경입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Meteor Hazard|Area", meta = (ClampMin = "0.01", UIMin = "0.01", ForceUnits = "cm"))
	float HazardRadius = 250.0f;

	/** 장판 활성화 뒤 첫 피해와 이후 반복 피해 사이의 간격입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Meteor Hazard|Damage", meta = (ClampMin = "0.01", UIMin = "0.01", ForceUnits = "s"))
	float DamageInterval = 1.0f;

	/** 생성 특수 패턴 이후 몇 번째 특수 패턴 요청에서 장판을 정리할지 나타냅니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Meteor Hazard|Lifetime", meta = (ClampMin = "1", UIMin = "1"))
	int32 SpecialPatternCleanupOffset = 2;

	/** 시간, 반경과 수명 값이 실행 가능한지 검사합니다 */
	bool IsDataValid(FString* OutValidationError = nullptr) const;

	/** 낙하 Niagara가 Telegraph 시작 후 재생을 시작할 시간을 반환합니다 */
	float GetFallEffectStartTime() const;

};

/** 예고 단계가 장판 전환 또는 조기 정리로 끝났음을 알립니다 */
DECLARE_MULTICAST_DELEGATE_OneParam(FRSMeteorHazardPreparationFinishedSignature, ERSBossMeteorHazardPreparationResult);

/** 플레이어를 추적한 뒤 고정 위치에 주기 피해 장판을 남기는 운석 Actor입니다 */
UCLASS(Blueprintable)
class RS_API ARSBossMeteorHazard : public AActor
{
	GENERATED_BODY()

public:
	ARSBossMeteorHazard();

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#if WITH_EDITOR
	/** 시간, 반경과 Niagara 크기 기준을 검사합니다 */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

public:
	/** FinishSpawning 전에 이번 실행이 추적할 대상을 전달합니다 */
	void Initialize(AActor* InTargetActor);

	/** 모든 표시와 Timer를 정리하고 Actor를 제거합니다 */
	void RequestCleanup();

	/** Ability가 예고 종료를 기다릴 델리게이트를 반환합니다 */
	FRSMeteorHazardPreparationFinishedSignature& OnPreparationFinished() { return PreparationFinishedEvent; }

	/** 현재 운석 상태를 반환합니다 */
	ERSBossMeteorHazardState GetMeteorHazardState() const { return MeteorHazardState; }

	/** 현재 추적 중인 바닥 위치를 반환합니다 */
	FVector GetCurrentTargetLocation() const { return CurrentTargetLocation; }

	/** 75%에서 고정한 장판 중심을 반환합니다 */
	FVector GetLockedImpactLocation() const { return LockedImpactLocation; }

	/** 낙하 Niagara 시작 시점을 지났는지 반환합니다 */
	bool HasStartedFallingEffect() const { return bFallingEffectStarted; }

#if WITH_DEV_AUTOMATION_TESTS
	/** 자동 테스트가 BeginPlay 전에 설정값을 교체합니다 */
	void SetDefinitionForTest(const FRSBossMeteorHazardDefinition& InDefinition) { MeteorHazardDefinition = InDefinition; }

	/** 자동 테스트가 첫 피해가 즉시 실행되지 않고 Timer로 예약됐는지 확인합니다 */
	bool IsDamageTimerActiveForTest() const;

	/** 자동 테스트가 첫 피해까지 남은 시간이 설정한 한 주기인지 확인합니다 */
	float GetDamageTimerRemainingForTest() const;

	/** 자동 테스트가 전역 Frame을 조작하지 않고 한 장판의 주기 피해 콜백을 실행합니다 */
	void ApplyHazardDamageForTest();
#endif

private:
	/** 대상 Capsule의 현재 바닥 위치를 갱신합니다 */
	bool UpdateTargetLocation();

	/** 낙하 Niagara를 한 번 활성화합니다 */
	void StartFallingEffect();

	/** 현재 목표 위치를 낙하 Niagara Component에 반영합니다 */
	void UpdateFallingEffectLocation(const FVector& TargetLocation);

	/** 현재 목표를 장판 중심으로 한 번 고정합니다 */
	void LockImpactLocation();

	/** 예고를 끝내고 고정 위치에서 장판과 피해 Timer를 시작합니다 */
	void BeginHazard();

	/** 현재 장판 안의 플레이어마다 1회 피해 Gameplay Event를 보냅니다 */
	void ApplyHazardDamage();

	/** Telegraph Handle만 회수합니다 */
	void HideTelegraph();

	/** 피해 Timer를 중지합니다 */
	void ClearTimers();

	/** 예고가 장판 또는 조기 정리로 끝났음을 한 번만 알립니다 */
	void BroadcastPreparationFinished(ERSBossMeteorHazardPreparationResult PreparationResult);

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Meteor Hazard", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Meteor Hazard", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraComponent> FallingNiagaraComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Meteor Hazard", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraComponent> HazardNiagaraComp;

	/** 보스 패턴 순번과 공용 정리 이벤트를 이 Actor의 정리 요청으로 변환합니다 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Meteor Hazard|Lifetime", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URSBossPersistentObjectLifetimeComponent> PersistentObjectLifetimeComp;

	/** 운석 예고와 장판의 시간, 범위와 수명 설정입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Meteor Hazard", meta = (AllowPrivateAccess = "true"))
	FRSBossMeteorHazardDefinition MeteorHazardDefinition;

	/** 100% 전까지 목표 위치에 표시할 낙하 Niagara입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Meteor Hazard|Presentation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraSystem> FallingNiagaraSystem;

	/** 100%에서 고정 위치에 시작할 장판 Niagara입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Meteor Hazard|Presentation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraSystem> HazardNiagaraSystem;

	/** 장판 Niagara 에셋이 Scale 1에서 표현하는 수평 반경입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Meteor Hazard|Presentation", meta = (AllowPrivateAccess = "true", ClampMin = "0.01", UIMin = "0.01", ForceUnits = "cm"))
	float HazardNiagaraBaseRadius = 50.0f;

	/** 장판 HitCheck가 플레이어 HurtBox를 찾을 채널입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Meteor Hazard|Damage", meta = (AllowPrivateAccess = "true"))
	TEnumAsByte<ECollisionChannel> PlayerTargetChannel = ECollisionChannel::ECC_GameTraceChannel1;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> TargetActor;

	UPROPERTY(Transient)
	TWeakObjectPtr<URSAttackTelegraphComponent> TelegraphComp;

	FRSMeteorHazardPreparationFinishedSignature PreparationFinishedEvent;
	FTimerHandle HazardDamageTimerHandle;
	FVector CurrentTargetLocation = FVector::ZeroVector;
	FVector LockedImpactLocation = FVector::ZeroVector;
	float TelegraphElapsedTime = 0.0f;
	int32 TelegraphHandle = INDEX_NONE;
	ERSBossMeteorHazardState MeteorHazardState = ERSBossMeteorHazardState::Tracking;
	bool bFallingEffectStarted = false;
	bool bPreparationFinishedBroadcast = false;
};
