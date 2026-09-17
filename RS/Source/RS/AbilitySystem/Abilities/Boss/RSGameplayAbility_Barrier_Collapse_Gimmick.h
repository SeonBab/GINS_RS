// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Containers/ArrayView.h"
#include "CoreMinimal.h"
#include "Combat/RSCombatFunctionLibrary.h"
#include "RSBaseGameplayAbility_BossPattern.h"
#include "RSNiagaraSpawnDefinition.h"
#include "RSGameplayAbility_Barrier_Collapse_Gimmick.generated.h"

class ARSBossCharacter;
class ARSBossController;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UAnimMontage;
class UNiagaraComponent;
class URSAbilityTask_BossFacing;

#if WITH_EDITOR
class FDataValidationContext;
#endif

/**
 * 색 구분 없는 네 방어막 중 남아 있는 아무 곳에나 들어가면 안전하고, 안전했던 자리의 방어막이 파괴되는 메인 기믹입니다
 * 순서 암기 기믹보다 쉬운 버전이며 기억과 색 대응 부담만 덜어내고 이동 요구는 그대로 둡니다
 */
UCLASS(Abstract, Blueprintable)
class RS_API URSGameplayAbility_Barrier_Collapse_Gimmick : public URSBaseGameplayAbility_BossPattern
{
	GENERATED_BODY()

public:
	URSGameplayAbility_Barrier_Collapse_Gimmick();

	/** 패턴 원점을 기준으로 네 방어막 중심을 45도부터 90도 간격으로 계산합니다 */
	static void CalculateBarrierZoneCenters(const FVector& PatternCenter, float DistanceFromCenterValue, TArray<FVector>& OutZoneCenters);

	/** 위치를 포함하는 첫 방어막의 인덱스를 반환하며 어느 방어막에도 없으면 `INDEX_NONE`을 반환합니다 */
	static int32 FindSafeZoneIndex(TConstArrayView<FVector> ZoneCenters, const FVector& Location, float SafeRadiusValue);

	/**
	 * 한 번의 판정에서 파괴할 방어막과 실패한 대상을 함께 확정합니다
	 * 판정 도중에 방어막을 지우면 같은 방어막에 있던 다음 대상이 실패로 바뀌므로 파괴는 모든 대상을 판정한 뒤로 미룹니다
	 * 파괴 대상 인덱스는 중복 없이 오름차순으로 반환하므로 호출자는 뒤에서부터 제거합니다
	 */
	static void ResolveHitCheck(TConstArrayView<FVector> ZoneCenters, TConstArrayView<FVector> TargetLocations, float SafeRadiusValue, TArray<int32>& OutDestroyedZoneIndices, TArray<int32>& OutFailedTargetIndices);

	/**
	 * 공격 Beam이 사용할 패턴 Transform을 만듭니다
	 * 보스가 매 공격마다 플레이어를 향해 돌기 때문에 위치만 패턴 원점에 고정하고 회전은 현재 수평 전방으로 갱신하며, 전방을 정규화할 수 없으면 시작 회전을 유지합니다
	 */
	static FTransform MakeAttackBeamPatternTransform(const FTransform& PatternTransform, const FVector& CurrentForward);

	/** 방어막 거리와 반지름이 유한한 양수이고 인접한 두 원이 겹치거나 맞닿지 않는지 검사합니다 */
	static bool IsBarrierFieldValid(float DistanceFromCenterValue, float SafeRadiusValue, FString* OutValidationError = nullptr);

protected:
	/** 시작 위치를 확정하고 방어막과 공격 횟수 예고를 시작합니다 */
	virtual void BeginPatternTimeline() override;

	/** 모든 종료 경로에서 이벤트 대기와 방어막 표현을 정리합니다 */
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

#if WITH_EDITOR
	/** 공간, Montage Notify와 Niagara 설정이 실행 계약에 맞는지 검사합니다 */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	struct FRSBarrierCollapseZoneRuntimeState
	{
		FVector Center = FVector::ZeroVector;
		TWeakObjectPtr<UNiagaraComponent> Presentation;
	};

	/** 고정된 시작 위치를 기준으로 판정에 사용할 네 방어막 상태를 생성합니다 */
	void CreateBarrierField();

	/** 생성된 네 방어막 상태에 대응하는 반복 Niagara를 생성합니다 */
	bool CreateBarrierPresentations();

	/** 지정한 방어막만 제거하며 인덱스는 오름차순 입력을 전제로 뒤에서부터 지웁니다 */
	void DestroyBarrierZonesAt(TConstArrayView<int32> ZoneIndices);

	/** 남아 있는 방어막 표현을 모두 제거합니다 */
	void DestroyBarrierZones();

	/** 판정 디버그가 켜져 있을 때만 남아 있는 방어막의 안전 반지름을 바닥 원으로 그립니다 */
	void DrawDebugBarrierZones(float LifeTime) const;

	/** 안광 이벤트 대기를 먼저 열고 예고 Montage를 재생합니다 */
	void StartPreviewMontage();

	/** 현재 공격의 이벤트 대기를 먼저 열고 공격 Montage를 재생합니다 */
	void StartAttackMontage();

	/**
	 * 공격 Montage와 함께 보스를 현재 타겟 쪽으로 돌리기 시작합니다
	 * 판정이 전 방향이라 조준 정확도가 결과를 바꾸지 않으므로 완료를 기다리지 않고 칼을 드는 동안 회전을 겹칩니다
	 */
	void StartAttackFacing();

	/** 보스 캐릭터와 Controller를 함께 확인합니다 */
	bool GetBossContext(ARSBossCharacter*& OutBossCharacter, ARSBossController*& OutBossController) const;

	/** 현재 Montage용 Gameplay Event Task를 끝냅니다 */
	void EndGameplayEventTasks();

	/** 예고 구간의 안광 이벤트를 Niagara 재생으로 변환합니다 */
	UFUNCTION()
	void HandleEyeNiagaraEvent(FGameplayEventData Payload);

	/** 공격 구간의 칼 점멸 이벤트를 Niagara 재생으로 변환합니다 */
	UFUNCTION()
	void HandleSwordNiagaraEvent(FGameplayEventData Payload);

	/** 현재 공격의 판정 이벤트를 처리합니다 */
	UFUNCTION()
	void HandleHitCheckEvent(FGameplayEventData Payload);

	/** Beam을 보스 전방의 설정 위치에 재생합니다 */
	void PlayBeamPresentation();

	/** 예고가 정상 완료되면 이벤트 개수를 확인하고 첫 공격을 시작합니다 */
	UFUNCTION()
	void HandlePreviewMontageCompleted();

	/** 공격이 정상 완료되면 계약을 확인하고 다음 공격 또는 패턴 종료로 진행합니다 */
	UFUNCTION()
	void HandleAttackMontageCompleted();

	/** Montage가 중단되거나 재생에 실패하면 패턴을 취소합니다 */
	UFUNCTION()
	void HandleMontageCancelled();

	/** 고정 범위의 플레이어를 판정해 실패한 대상에게 피해를 주고 안전했던 방어막을 파괴합니다 */
	void ExecuteCurrentHitCheck();

	/** 설정 오류 또는 런타임 이벤트 계약 위반을 기록하고 패턴을 취소합니다 */
	void CancelForInvalidRuntime(const TCHAR* Reason);

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Barrier Collapse|Animation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimMontage> RoarMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Barrier Collapse|Animation", meta = (AllowPrivateAccess = "true", ClampMin = "0.01", UIMin = "0.01"))
	float RoarMontagePlayRate = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Barrier Collapse|Animation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimMontage> AttackMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Barrier Collapse|Animation", meta = (AllowPrivateAccess = "true", ClampMin = "0.01", UIMin = "0.01"))
	float AttackMontagePlayRate = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Barrier Collapse|Presentation", meta = (AllowPrivateAccess = "true"))
	FRSNiagaraSpawnDefinition EyeNiagara;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Barrier Collapse|Presentation", meta = (AllowPrivateAccess = "true"))
	FRSNiagaraSpawnDefinition SwordNiagara;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Barrier Collapse|Presentation", meta = (AllowPrivateAccess = "true"))
	FRSNiagaraSpawnDefinition BeamNiagara;

	/** 패턴 시작 시 고정한 보스 발밑 기준의 Beam 생성 위치이며 X가 보스 전방, Z가 바닥에서의 높이입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Barrier Collapse|Presentation", meta = (AllowPrivateAccess = "true", ForceUnits = "cm"))
	FVector BeamOffset = FVector::ZeroVector;

	/** 보스 중심에서 각 방어막 중심까지의 거리입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Barrier Collapse|Barrier", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm"))
	float DistanceFromCenter = 0.0f;

	/** 플레이어 중심이 포함되어야 하는 방어막 안전 반지름입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Barrier Collapse|Barrier", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm"))
	float SafeRadius = 0.0f;

	/** 방어막 하나의 반복 표현이며 네 곳에 같은 정의로 생성합니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Barrier Collapse|Barrier", meta = (AllowPrivateAccess = "true"))
	FRSNiagaraSpawnDefinition BarrierNiagara;

	/** 공격 중 임시로 적용할 Boss Yaw 회전 속도입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Barrier Collapse|Aim", meta = (AllowPrivateAccess = "true", ClampMin = "0.01", UIMin = "0.01", ForceUnits = "deg/s"))
	float AimRotationSpeed = 150.0f;

	/** Snapshot 방향을 향했다고 판정할 절대 Yaw 오차입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Barrier Collapse|Aim", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0", ForceUnits = "deg"))
	float AimYawTolerance = 3.0f;

	/** Facing이 끝났을 때 Target이 Snapshot에서 이 거리보다 멀어졌으면 한 번 다시 조준합니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Barrier Collapse|Aim", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm"))
	float TargetDriftTolerance = 100.0f;

	/** Target이 계속 움직여도 회전을 끝낼 공격 한 번당 최대 시간입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Barrier Collapse|Aim", meta = (AllowPrivateAccess = "true", ClampMin = "0.01", UIMin = "0.01", ForceUnits = "s"))
	float MaxAimDuration = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Barrier Collapse|Hit", meta = (AllowPrivateAccess = "true"))
	FRSCombatShape AttackShape;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Barrier Collapse|Hit", meta = (AllowPrivateAccess = "true"))
	TEnumAsByte<ECollisionChannel> TargetChannel = ECollisionChannel::ECC_GameTraceChannel1;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> PresentationEventTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> HitCheckEventTask;

	TArray<FRSBarrierCollapseZoneRuntimeState> BarrierZones;

	UPROPERTY(Transient)
	FTransform PatternCenterTransform = FTransform::Identity;

	UPROPERTY(Transient)
	int32 PreviewEventCount = 0;

	UPROPERTY(Transient)
	int32 AttackIndex = 0;

	UPROPERTY(Transient)
	bool bReceivedAttackPresentation = false;

	UPROPERTY(Transient)
	bool bReceivedHitCheck = false;

	/** Task 종료 Callback이 EndAbility에 재진입하지 않게 막습니다 */
	bool bIsEndingAbility = false;
};
