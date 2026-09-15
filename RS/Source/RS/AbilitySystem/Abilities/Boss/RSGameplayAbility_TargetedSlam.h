#pragma once

#include "CoreMinimal.h"
#include "Combat/RSCombatFunctionLibrary.h"
#include "Components/RSAttackTelegraphComponent.h"
#include "Engine/EngineTypes.h"
#include "RSBaseGameplayAbility_BossPattern.h"
#include "RSGameplayAbility_TargetedSlam.generated.h"

class ARSBossCharacter;
class ARSBossController;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitDelay;
class UAnimMontage;
class UGameplayEffect;
class URSAbilityTask_ObserveFacing;

/** 현재 Targeted Slam 활성화가 어느 실행 단계인지 나타냅니다 */
enum class ERSTargetedSlamState : uint8
{
	Inactive,
	PreAiming,
	Attacking
};

/**
 * 각 Strike에서 현재 보스 TargetActor의 위치를 향해 회전한 뒤 확정한 Cone 공간을 예고하고 내려찍습니다
 * Target과 Boss가 이후 움직여도 해당 Strike의 Telegraph와 판정은 공격 확정 시점의 같은 Transform을 사용합니다
 */
UCLASS(Abstract, Blueprintable)
class RS_API URSGameplayAbility_TargetedSlam : public URSBaseGameplayAbility_BossPattern
{
	GENERATED_BODY()

public:
	/** 한 번 또는 두 번 내려찍는 패턴의 개발용 기본값을 구성합니다 */
	URSGameplayAbility_TargetedSlam();

protected:
	/** 현재 TargetActor와 설정을 검증하고 Pre-Aim을 시작합니다 */
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	/** 종료 경로와 관계없이 Telegraph, Focus, 회전 설정과 애니메이션 상태를 정리합니다 */
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

#if WITH_EDITOR
	/** 한 Strike용 Montage, Cone과 실행 수치가 현재 구현 계약에 맞는지 검사합니다 */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	/** 현재 Strike의 Target과 Pre-Aim 상태를 새로 만들고 Facing 관찰을 시작합니다 */
	void BeginStrike();

	/** Pre-Aim에서 현재 Facing 결과를 받아 timeout, 방향과 drift 순서로 공격 확정 여부를 판단합니다 */
	UFUNCTION()
	void HandleFacingUpdated(bool bHasFacingDirection, bool bIsWithinYawTolerance, float YawErrorDegrees);

	/** 공격 방향과 위치를 고정하고 현재 Strike의 Telegraph와 Montage를 시작합니다 */
	void ConfirmAttack();

	/** 현재 Strike의 일회성 상태를 초기화하고 Montage를 시작합니다 */
	void StartStrike();

	/** 현재 Strike의 ImpactDelay가 끝나면 고정한 공격 공간에 판정을 실행합니다 */
	UFUNCTION()
	void HandleImpactDelayFinished();

	/** Montage가 정상 완료되면 Impact 완료 여부와 함께 현재 Strike 종료를 시도합니다 */
	UFUNCTION()
	void HandleAttackMontageCompleted();

	/** Montage가 중단되거나 Task가 취소되면 Ability를 취소 종료합니다 */
	UFUNCTION()
	void HandleAttackMontageInterrupted();

	/** Impact와 Montage가 모두 완료됐으면 다음 Strike 또는 Ability 종료로 전환합니다 */
	void TryFinishStrike();

	/** 재사용되는 Ability 인스턴스에서 현재 Strike의 상태를 초기화합니다 */
	void ResetStrikeTransientState();

	/** 현재 ActorInfo에서 Targeted Slam 실행에 필요한 Boss Character와 Controller를 반환합니다 */
	bool GetBossContext(ARSBossCharacter*& OutBossCharacter, ARSBossController*& OutBossController) const;

private:
	/** 한 활성화에서 실행할 Strike 수이며 현재 패턴은 1회형과 2회형만 지원합니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Targeted Slam", meta = (AllowPrivateAccess = "true", ClampMin = "1", ClampMax = "2", UIMin = "1", UIMax = "2"))
	int32 StrikeCount = 1;

	/** Telegraph와 HitCheck가 함께 사용할 Cone 형상입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Targeted Slam|Hit", meta = (AllowPrivateAccess = "true"))
	FRSCombatShape AttackShape;

	/** 한 Strike에서 한 번 재생할 Montage입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Targeted Slam|Animation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimMontage> AttackMontage;

	/** Montage 재생 속도이며 Telegraph Fill 시간 계산에도 같은 값이 적용됩니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Targeted Slam|Animation", meta = (AllowPrivateAccess = "true", ClampMin = "0.01", UIMin = "0.01"))
	float MontagePlayRate = 1.0f;

	/** Strike 시작부터 실제 판정까지의 시간이며 Telegraph의 두 선행 시간이 이 값을 기준으로 계산됩니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Targeted Slam|Timing", meta = (AllowPrivateAccess = "true", ClampMin = "0.01", UIMin = "0.01", ForceUnits = "s"))
	float ImpactDelay = 1.35f;

	/** Telegraph의 Fill 완료와 표시 소멸이 판정보다 각각 얼마나 빠른지입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Targeted Slam|Timing", meta = (AllowPrivateAccess = "true"))
	FRSTelegraphLeadTime TelegraphLeadTime;

	/** Pre-Aim 동안 임시로 적용할 Boss Yaw 회전 속도입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Targeted Slam|Aim", meta = (AllowPrivateAccess = "true", ClampMin = "0.01", UIMin = "0.01", ForceUnits = "deg/s"))
	float AimRotationSpeed = 150.0f;

	/** Snapshot 방향을 향했다고 판정할 절대 Yaw 오차입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Targeted Slam|Aim", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0", ForceUnits = "deg"))
	float AimYawTolerance = 3.0f;

	/** Facing이 끝났을 때 Target이 Snapshot에서 이 거리보다 멀어졌으면 한 번 다시 조준합니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Targeted Slam|Aim", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm"))
	float TargetDriftTolerance = 100.0f;

	/** Target이 계속 움직여도 공격을 반드시 확정할 Pre-Aim 최대 시간입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Targeted Slam|Aim", meta = (AllowPrivateAccess = "true", ClampMin = "0.01", UIMin = "0.01", ForceUnits = "s"))
	float MaxAimDuration = 1.5f;

	/** Cone에 포함된 대상에게 적용할 피해량입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Targeted Slam|Hit", meta = (AllowPrivateAccess = "true"))
	FScalableFloat Damage = 15.0f;

	/** 판정에 걸린 대상에게 적용할 대미지 GameplayEffect입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Targeted Slam|Hit", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	/** 보스 공격이 노릴 PlayerHurtBox Trace Channel입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Targeted Slam|Hit", meta = (AllowPrivateAccess = "true"))
	TEnumAsByte<ECollisionChannel> TargetChannel = ECollisionChannel::ECC_GameTraceChannel1;

	/** 판정에 걸린 대상에게 요청할 피격 반응이며 첫 통합 검증은 None입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Targeted Slam|Reaction", meta = (AllowPrivateAccess = "true"))
	FRSHitReactionDefinition Reaction;

private:
	/** 한 활성화에서 고정한 Controller Target이며 Pre-Aim 중 새 대상을 선택하지 않습니다 */
	TWeakObjectPtr<AActor> AimTargetActor;

	/** 현재 Pre-Aim이 향하는 고정 월드 위치입니다 */
	FVector AimSnapshotLocation = FVector::ZeroVector;

	/** Telegraph와 HitCheck가 함께 사용하는 공격 확정 시점의 월드 Transform입니다 */
	FTransform LockedAttackTransform = FTransform::Identity;

	/** 현재 Strike Pre-Aim이 시작된 World gameplay time입니다 */
	float PreAimStartTime = 0.0f;

	/** 현재 활성화에서 실행 중인 Strike 인덱스입니다 */
	int32 CurrentStrikeIndex = 0;

	/** 현재 Strike의 Impact 판정을 이미 실행했는지 나타냅니다 */
	bool bHasExecutedImpact = false;

	/** 현재 Strike의 Montage가 정상 완료됐는지 나타냅니다 */
	bool bHasCompletedMontage = false;

	/** Ability가 설정한 외부 상태를 되돌릴 수 있도록 원래 회전 설정을 저장했는지 나타냅니다 */
	bool bHasSavedRotationSettings = false;

	/** Gameplay 우선순위 Focus를 Ability가 설정했는지 나타냅니다 */
	bool bHasAppliedGameplayFocus = false;

	/** EndAbility 정리 중 Task Callback 재진입을 막습니다 */
	bool bIsCleaningUp = false;

	/** 이번 활성화가 Commit되어 외부 상태 정리 책임을 획득했는지 나타냅니다 */
	bool bHasCommittedActivation = false;

	/** Ability 시작 전 CharacterMovement의 회전 속도입니다 */
	FRotator OriginalRotationRate = FRotator::ZeroRotator;

	/** Ability 시작 전 Controller 목표 회전을 사용했는지 나타냅니다 */
	bool bOriginalUseControllerDesiredRotation = false;

	/** 현재 실행 단계입니다 */
	ERSTargetedSlamState State = ERSTargetedSlamState::Inactive;

	/** Pre-Aim 동안 Facing을 프레임 단위로 관찰하는 Task입니다 */
	UPROPERTY(Transient)
	TObjectPtr<URSAbilityTask_ObserveFacing> ObserveFacingTask;

	/** 현재 Strike 시작부터 Impact 판정까지 기다리는 Task입니다 */
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitDelay> ImpactDelayTask;

	/** 현재 Strike의 Montage 수명을 관찰하는 Task입니다 */
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;
};
