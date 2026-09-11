#pragma once

#include "CoreMinimal.h"
#include "Combat/RSCombatFunctionLibrary.h"
#include "Engine/EngineTypes.h"
#include "RSBaseGameplayAbility.h"
#include "RSGameplayAbility_SequentialSweepExplosion.generated.h"

class ARSBossCharacter;
class ARSBossController;
class UAbilityTask_PlayMontageAndWait;
class UAnimMontage;
class UGameplayEffect;
class URSAbilityTask_ObserveElapsedTime;
class URSAbilityTask_ObserveFacing;

/** 순차 스윕 폭발 패턴의 공간, 시간, 피해와 반응 설정입니다 */
USTRUCT(BlueprintType)
struct FRSSequentialSweepExplosionDefinition
{
	GENERATED_BODY()

	/** 보스 중심에서 부채꼴 바깥 경계까지의 거리입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Sequential Sweep Explosion|Hit", meta = (ClampMin = "0.01", UIMin = "0.01", ForceUnits = "cm"))
	float OuterRadius = 800.0f;

	/** 모든 부채꼴이 합쳐서 시계 방향으로 덮을 전체 공격 각도입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Sequential Sweep Explosion|Hit", meta = (ClampMin = "0.01", ClampMax = "360.0", UIMin = "0.01", UIMax = "360.0", ForceUnits = "deg"))
	float TotalSweepAngleDegrees = 180.0f;

	/** 전체 공격 각도를 균등 분할할 부채꼴 개수입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Sequential Sweep Explosion|Hit", meta = (ClampMin = "1", UIMin = "1"))
	int32 SectorCount = 6;

	/** 고정 Boss Forward에서 첫 부채꼴의 시작 경계까지의 각도입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Sequential Sweep Explosion|Hit", meta = (ForceUnits = "deg"))
	float StartAngleOffsetDegrees = -90.0f;

	/** Montage 재생 요청부터 첫 부채꼴 폭발까지의 시간입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Sequential Sweep Explosion|Timing", meta = (ClampMin = "0.01", UIMin = "0.01", ForceUnits = "s"))
	float FirstExplosionDelay = 1.5f;

	/** 이전 부채꼴과 다음 부채꼴의 폭발 시점 사이 간격입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Sequential Sweep Explosion|Timing", meta = (ClampMin = "0.01", UIMin = "0.01", ForceUnits = "s"))
	float SectorExplosionInterval = 0.15f;

	/** 모든 부채꼴이 공유할 피해량입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Sequential Sweep Explosion|Hit")
	FScalableFloat Damage = 10.0f;

	/** 모든 부채꼴이 공유할 피격 반응입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Sequential Sweep Explosion|Reaction")
	FRSHitReactionDefinition Reaction;

	/** 런타임 계산에 사용할 수 있는 값인지 검사합니다 */
	bool IsDataValid(FString* OutValidationError = nullptr) const;
};

/** 현재 순차 스윕 폭발 활성화가 어느 실행 단계인지 나타냅니다 */
enum class ERSSequentialSweepExplosionState : uint8
{
	Inactive,
	PreAiming,
	Attacking
};

/** Target Aim 뒤 고정한 부채꼴을 누적 예고하고 시계 방향으로 하나씩 폭발시킵니다 */
UCLASS(Abstract, Blueprintable)
class RS_API URSGameplayAbility_SequentialSweepExplosion : public URSBaseGameplayAbility
{
	GENERATED_BODY()

public:
	URSGameplayAbility_SequentialSweepExplosion();

protected:
	/** 현재 Target과 설정을 검증하고 Aim을 시작합니다 */
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	/** 종료 경로와 관계없이 예고, 시간 Task, Focus, 회전 설정과 애니메이션 상태를 정리합니다 */
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

#if WITH_EDITOR
	/** 공간·시간·Aim·Damage와 필수 GameplayEffect 설정을 검사합니다 */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	/** 현재 Target의 Snapshot을 향한 Facing 관찰을 시작합니다 */
	void BeginAim();

	/** Aim 결과와 timeout을 평가해 공격 확정 여부를 판단합니다 */
	UFUNCTION()
	void HandleFacingUpdated(bool bHasFacingDirection, bool bIsWithinYawTolerance, float YawErrorDegrees);

	/** 현재 Boss 바닥과 Facing을 고정하고 공격 타임라인을 시작합니다 */
	void ConfirmAttack();

	/** Montage를 요청하고 같은 시점부터 누적 예고와 순차 폭발 시간을 시작합니다 */
	void StartAttackTimeline();

	/** 선택적 Montage를 요청하며 재생 실패는 공격 타임라인을 취소하지 않습니다 */
	void RequestAttackMontage();

	/** Montage가 정상 완료되면 Task 참조만 정리합니다 */
	UFUNCTION()
	void HandleAttackMontageCompleted();

	/** 재생 중이던 Montage가 강제로 중단되면 Ability를 취소합니다 */
	UFUNCTION()
	void HandleAttackMontageInterrupted();

	/** 경과 시간까지 예정된 모든 예고와 폭발을 순서대로 보충 처리합니다 */
	UFUNCTION()
	void HandleTimelineElapsedTimeUpdated(float ElapsedTime);

	/** 지정한 부채꼴 Telegraph를 완성 상태로 표시합니다 */
	bool ShowWarningSector(int32 SectorIndex);

	/** 지정한 부채꼴을 판정하고 대응 Telegraph를 제거합니다 */
	bool ExecuteSectorExplosion(int32 SectorIndex);

	/** 현재 Ability가 만든 모든 Telegraph Handle을 회수합니다 */
	void HideAllWarningSectors();

	/** 현재 ActorInfo에서 필요한 Boss Character와 Controller를 반환합니다 */
	bool GetBossContext(ARSBossCharacter*& OutBossCharacter, ARSBossController*& OutBossController) const;

	/** 재사용되는 Ability 인스턴스의 실행 상태를 초기화합니다 */
	void ResetTransientState();

protected:
	/** 순차 스윕 폭발의 공간, 시간, 피해와 반응 설정입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Sequential Sweep Explosion")
	FRSSequentialSweepExplosionDefinition PatternDefinition;

	/** Aim 뒤 한 번 재생을 요청할 선택적 Montage입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Sequential Sweep Explosion|Animation")
	TObjectPtr<UAnimMontage> AttackMontage;

	/** Montage 재생 속도이며 독립 공격 타임라인의 시간에는 영향을 주지 않습니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Sequential Sweep Explosion|Animation", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float MontagePlayRate = 1.0f;

	/** Aim 동안 임시로 적용할 Boss Yaw 회전 속도입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Sequential Sweep Explosion|Aim", meta = (ClampMin = "0.01", UIMin = "0.01", ForceUnits = "deg/s"))
	float AimRotationSpeed = 150.0f;

	/** Snapshot 방향을 향했다고 판정할 절대 Yaw 오차입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Sequential Sweep Explosion|Aim", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "deg"))
	float AimYawTolerance = 3.0f;

	/** Facing 완료 시 Target이 Snapshot에서 이 거리보다 멀어졌으면 다시 조준합니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Sequential Sweep Explosion|Aim", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm"))
	float TargetDriftTolerance = 100.0f;

	/** 초과하면 현재 Facing으로 공격을 강제 확정할 Aim 최대 시간입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Sequential Sweep Explosion|Aim", meta = (ClampMin = "0.01", UIMin = "0.01", ForceUnits = "s"))
	float MaxAimDuration = 1.5f;

	/** 판정에 걸린 대상에게 적용할 대미지 GameplayEffect입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Sequential Sweep Explosion|Hit")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	/** 보스 공격이 노릴 PlayerHurtBox Trace Channel입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Sequential Sweep Explosion|Hit")
	TEnumAsByte<ECollisionChannel> TargetChannel = ECollisionChannel::ECC_GameTraceChannel1;

private:
	TWeakObjectPtr<AActor> AimTargetActor;
	FVector AimSnapshotLocation = FVector::ZeroVector;
	FTransform LockedAttackTransform = FTransform::Identity;
	float PreAimStartTime = 0.0f;
	int32 NextWarningSectorIndex = 0;
	int32 NextExplosionSectorIndex = 0;
	TArray<int32> WarningSectorHandles;
	TSet<TWeakObjectPtr<AActor>> HitActors;
	bool bHasSavedRotationSettings = false;
	bool bHasAppliedGameplayFocus = false;
	bool bHasCommittedActivation = false;
	bool bIsCleaningUp = false;
	FRotator OriginalRotationRate = FRotator::ZeroRotator;
	bool bOriginalUseControllerDesiredRotation = false;
	ERSSequentialSweepExplosionState State = ERSSequentialSweepExplosionState::Inactive;

	UPROPERTY(Transient)
	TObjectPtr<URSAbilityTask_ObserveFacing> ObserveFacingTask;

	UPROPERTY(Transient)
	TObjectPtr<URSAbilityTask_ObserveElapsedTime> TimelineTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;
};
