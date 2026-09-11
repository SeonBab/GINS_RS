// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Combat/RSArmSwingMath.h"
#include "Combat/RSCombatFunctionLibrary.h"
#include "Engine/EngineTypes.h"
#include "RSBaseGameplayAbility_BossPattern.h"
#include "RSGameplayAbility_ArmSwing.generated.h"

class ARSBossCharacter;
class ARSBossController;
class UAbilityTask_PlayMontageAndWait;
class UAnimMontage;
class UGameplayEffect;
class URSAbilityTask_ObserveFacing;
class URSAbilityTask_ObserveAttackWindow;

/** Arm Swing의 한 Side가 사용할 필수 Montage와 회전 경로입니다 */
USTRUCT(BlueprintType)
struct FRSArmSwingVariantDefinition
{
	GENERATED_BODY()

	/** 이 Variant가 재생할 독립 Montage입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Arm Swing|Animation")
	TObjectPtr<UAnimMontage> AttackMontage;

	/** 고정된 공격 기준 Yaw에서 Box가 시작되는 상대 각도입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Arm Swing|Hit", meta = (ForceUnits = "deg"))
	float StartYawOffset = 0.0f;

	/** 공격 Window 전체에서 Box가 회전할 각도이며 부호가 진행 방향을 결정합니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Arm Swing|Hit", meta = (ClampMin = "-360.0", ClampMax = "360.0", UIMin = "-180.0", UIMax = "180.0", ForceUnits = "deg"))
	float SweepAngleDegrees = 160.0f;

	/** 공간 계산에 전달할 경로 정의를 반환합니다 */
	FRSArmSwingPathDefinition GetPathDefinition() const;
};

/** 현재 Arm Swing 활성화가 어느 실행 단계인지 나타냅니다 */
enum class ERSArmSwingState : uint8
{
	Inactive,
	PreAiming,
	Attacking
};

/** 플레이어를 향해 회전한 뒤 선택한 Left 또는 Right Arm Swing Montage를 고정 방향으로 재생합니다 */
UCLASS(Abstract, Blueprintable)
class RS_API URSGameplayAbility_ArmSwing : public URSBaseGameplayAbility_BossPattern
{
	GENERATED_BODY()

public:
	URSGameplayAbility_ArmSwing();

	/** Attack Window 구간의 Box 회전 진행률을 정의하는 Montage 내부 Curve 이름입니다 */
	static const FName SweepProgressCurveName;

protected:
	/** 필수 Variant와 Boss Target을 검증하고 Left 또는 Right를 선택해 Pre-Aim을 시작합니다 */
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	/** 종료 경로와 관계없이 Focus, 회전 설정과 선택한 Montage의 애니메이션 상태를 정리합니다 */
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

#if WITH_EDITOR
	/** 두 Variant, 공통 공격 데이터와 Slow/Fast leaf Tag가 실행 계약에 맞는지 검사합니다 */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	/** Pre-Aim에서 현재 Facing 결과를 받아 방향과 Target drift를 확인합니다 */
	UFUNCTION()
	void HandleFacingUpdated(bool bHasFacingDirection, bool bIsWithinYawTolerance, float YawErrorDegrees);

	/** Capsule 발밑 기준 공격 Transform을 고정하고 선택한 Montage를 시작합니다 */
	void ConfirmAttack();

	/** 선택한 필수 Montage를 재생하고 완료·중단 수명을 관찰합니다 */
	void StartAttackMontage();

	/** 현재 Montage Position을 Telegraph 준비 구간의 0~1 채움률로 반영합니다 */
	UFUNCTION()
	void HandleAttackMontagePositionUpdated(float MontagePosition);

	/** Attack Window 시작 순간의 Box를 판정하고 Window 적중 기록을 초기화합니다 */
	UFUNCTION()
	void HandleAttackWindowBegan();

	/** 전달된 진행률 구간을 적응형 Substep으로 나눠 회전 Box를 판정합니다 */
	UFUNCTION()
	void HandleAttackWindowAdvanced(float PreviousAlpha, float CurrentAlpha);

	/** Attack Window가 끝까지 정상 진행했음을 기록합니다 */
	UFUNCTION()
	void HandleAttackWindowEnded();

	/** Attack Window 구성이 잘못되거나 Montage가 중단되면 Ability를 취소합니다 */
	UFUNCTION()
	void HandleAttackWindowInvalidated();

	/** Attack Window의 0~1 진행률을 Montage Curve가 정의한 회전 진행률로 변환합니다 */
	bool TryEvaluateSweepProgress(float WindowAlpha, float& OutSweepProgress) const;

	/** 한 회전 진행률의 Box를 조회하고 처음 검출된 대상에게 Damage와 Knockdown을 요청합니다 */
	bool ExecuteAttackBoxSample(float SweepProgress);

	/** 선택한 Montage가 정상 완료되면 Ability를 성공 종료합니다 */
	UFUNCTION()
	void HandleAttackMontageCompleted();

	/** 선택한 Montage가 중단되거나 Task가 취소되면 Ability를 취소 종료합니다 */
	UFUNCTION()
	void HandleAttackMontageInterrupted();

	/** 현재 ActorInfo에서 Arm Swing 실행에 필요한 Boss Character와 Controller를 반환합니다 */
	bool GetBossContext(ARSBossCharacter*& OutBossCharacter, ARSBossController*& OutBossController) const;

	/** Variant의 Montage·경로와 Attack Window 구성이 유효한지 검사합니다 */
	bool IsVariantRuntimeValid(const FRSArmSwingVariantDefinition& Variant, int32* OutAttackWindowCount = nullptr) const;

	/** Variant Montage의 진행률 Curve가 Attack Window 구간에서 0에서 1까지 단조 증가하는지 검사합니다 */
	bool IsVariantSweepCurveValid(const FRSArmSwingVariantDefinition& Variant, FString* OutValidationError = nullptr) const;

	/** 재사용되는 Ability 인스턴스의 실행 상태를 초기화합니다 */
	void ResetTransientState();

private:
	/** 이 GA가 무작위로 선택할 왼쪽 휘두르기입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Arm Swing|Variant", meta = (AllowPrivateAccess = "true"))
	FRSArmSwingVariantDefinition LeftVariant;

	/** 이 GA가 무작위로 선택할 오른쪽 휘두르기입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Arm Swing|Variant", meta = (AllowPrivateAccess = "true"))
	FRSArmSwingVariantDefinition RightVariant;

	/** 선택한 Montage의 재생 속도이며 Slow와 Fast GA를 구분하는 값입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Arm Swing|Animation", meta = (AllowPrivateAccess = "true", ClampMin = "0.01", UIMin = "0.01"))
	float MontagePlayRate = 1.0f;

	/** Left와 Right가 공유하는 회전형 공격 Box 크기입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Arm Swing|Hit", meta = (AllowPrivateAccess = "true"))
	FRSArmSwingBoxDefinition AttackBox;

	/** 판정에 걸린 대상에게 적용할 피해량입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Arm Swing|Hit", meta = (AllowPrivateAccess = "true"))
	FScalableFloat Damage = 15.0f;

	/** 판정에 걸린 대상에게 적용할 대미지 GameplayEffect입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Arm Swing|Hit", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	/** Arm Swing이 노릴 PlayerHurtBox Trace Channel입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Arm Swing|Hit", meta = (AllowPrivateAccess = "true"))
	TEnumAsByte<ECollisionChannel> TargetChannel = ECollisionChannel::ECC_GameTraceChannel1;

	/** 적중 대상에게 팔 진행 방향 넉다운을 요청할 공통 수치입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Arm Swing|Reaction", meta = (AllowPrivateAccess = "true"))
	FRSHitReactionDefinition Reaction;

	/** Pre-Aim 동안 임시로 적용할 Boss Yaw 회전 속도입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Arm Swing|Aim", meta = (AllowPrivateAccess = "true", ClampMin = "0.01", UIMin = "0.01", ForceUnits = "deg/s"))
	float AimRotationSpeed = 150.0f;

	/** Snapshot 방향을 향했다고 판정할 절대 Yaw 오차입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Arm Swing|Aim", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0", ForceUnits = "deg"))
	float AimYawTolerance = 3.0f;

	/** Facing이 끝났을 때 Target이 Snapshot에서 이 거리보다 멀어졌으면 다시 조준합니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Arm Swing|Aim", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm"))
	float TargetDriftTolerance = 100.0f;

	/** 이 시간 안에 Facing을 완료하지 못하면 방향이 덜 맞은 채 공격하지 않고 취소합니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Arm Swing|Aim", meta = (AllowPrivateAccess = "true", ClampMin = "0.01", UIMin = "0.01", ForceUnits = "s"))
	float MaxAimDuration = 1.5f;

private:
	/** 한 활성화에서 50:50으로 선택하고 종료까지 유지할 Variant입니다 */
	const FRSArmSwingVariantDefinition* SelectedVariant = nullptr;

	/** 한 활성화에서 고정한 Controller Target이며 Pre-Aim 중 새 대상을 선택하지 않습니다 */
	TWeakObjectPtr<AActor> AimTargetActor;

	/** 현재 Pre-Aim이 향하는 고정 월드 위치입니다 */
	FVector AimSnapshotLocation = FVector::ZeroVector;

	/** 공격 확정 순간 Capsule 발밑과 Facing Yaw로 고정한 공격 Transform입니다 */
	FTransform LockedAttackTransform = FTransform::Identity;

	/** 현재 Pre-Aim이 시작된 World gameplay time입니다 */
	float PreAimStartTime = 0.0f;

	/** Ability가 설정한 외부 상태를 되돌릴 수 있도록 원래 회전 설정을 저장했는지 나타냅니다 */
	bool bHasSavedRotationSettings = false;

	/** Gameplay 우선순위 Focus를 Ability가 설정했는지 나타냅니다 */
	bool bHasAppliedGameplayFocus = false;

	/** EndAbility 정리 중 Task Callback 재진입을 막습니다 */
	bool bIsCleaningUp = false;

	/** Ability가 Commit되어 외부 상태 정리 책임을 획득했는지 나타냅니다 */
	bool bHasCommittedActivation = false;

	/** Ability 시작 전 CharacterMovement의 회전 속도입니다 */
	FRotator OriginalRotationRate = FRotator::ZeroRotator;

	/** Ability 시작 전 Controller 목표 회전을 사용했는지 나타냅니다 */
	bool bOriginalUseControllerDesiredRotation = false;

	/** 현재 실행 단계입니다 */
	ERSArmSwingState State = ERSArmSwingState::Inactive;

	/** 현재 Attack Window가 Begin 이후 End 이전인지 나타냅니다 */
	bool bIsAttackWindowActive = false;

	/** 선택한 Montage가 Attack Window 끝까지 정상 진행했는지 나타냅니다 */
	bool bHasCompletedAttackWindow = false;

	/** 한 Window에서 Substep 상한 도달 경고를 이미 남겼는지 나타냅니다 */
	bool bHasReportedSubstepLimit = false;

	/** 현재 환형 부채꼴 Telegraph를 개별 회수하기 위한 Component Handle입니다 */
	int32 ActiveTelegraphHandle = INDEX_NONE;

	/** Telegraph가 0으로 시작한 실제 Montage Position입니다 */
	float TelegraphStartPosition = 0.0f;

	/** Telegraph가 1이 되고 판정이 시작되는 Attack Window Position입니다 */
	float AttackWindowStartPosition = 0.0f;

	/** 판정과 회전 진행률이 끝나는 Attack Window Position입니다 */
	float AttackWindowEndPosition = 0.0f;

	/** 현재 Window에서 이미 Damage와 Knockdown을 요청한 Actor 집합입니다 */
	TSet<TWeakObjectPtr<AActor>> HitActors;

	/** Pre-Aim 동안 Facing을 프레임 단위로 관찰하는 Task입니다 */
	UPROPERTY(Transient)
	TObjectPtr<URSAbilityTask_ObserveFacing> ObserveFacingTask;

	/** 선택한 Montage의 완료와 중단 수명을 관찰하는 Task입니다 */
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	/** 선택한 Montage의 공용 Attack Window 진행률을 관찰하는 Task입니다 */
	UPROPERTY(Transient)
	TObjectPtr<URSAbilityTask_ObserveAttackWindow> AttackWindowTask;
};
