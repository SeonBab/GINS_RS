// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Combat/RSCombatFunctionLibrary.h"
#include "RSBaseGameplayAbility_BossPattern.h"
#include "RSNiagaraSpawnDefinition.h"
#include "RSGameplayAbility_Barrier_Memory_Gimmick.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UAnimMontage;
class UNiagaraComponent;

#if WITH_EDITOR
class FDataValidationContext;
#endif

/** 방어막과 예고 및 공격에서 사용하는 두 색입니다 */
UENUM(BlueprintType)
enum class ERSBarrierColor : uint8
{
	Red,
	Yellow
};

/** 보스 시작 위치를 기준으로 생성하는 네 방어막의 공통 설정입니다 */
USTRUCT(BlueprintType)
struct FRSBarrierFieldDefinition
{
	GENERATED_BODY()

	/** 거리와 반지름 및 두 색의 표현 정의가 방어막 필드 계약에 맞는지 검사합니다 */
	bool IsDataValid(FString* OutValidationError = nullptr) const;

	/** 보스 중심에서 각 방어막 중심까지의 거리입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Barrier", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm"))
	float DistanceFromCenter = 0.0f;

	/** 플레이어 중심이 포함되어야 하는 방어막 안전 반지름입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Barrier", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm"))
	float SafeRadius = 0.0f;

	/** 빨간 방어막의 반복 표현입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Barrier")
	FRSNiagaraSpawnDefinition RedNiagara;

	/** 노란 방어막의 반복 표현입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Barrier")
	FRSNiagaraSpawnDefinition YellowNiagara;
};

/** 안광으로 교대 색 순서를 예고한 뒤 같은 순서로 세 번 내려찍는 메인 기믹입니다 */
UCLASS(Abstract, Blueprintable)
class RS_API URSGameplayAbility_Barrier_Memory_Gimmick : public URSBaseGameplayAbility_BossPattern
{
	GENERATED_BODY()

public:
	URSGameplayAbility_Barrier_Memory_Gimmick();

	/** 첫 색과 인덱스로 빨강과 노랑이 교대하는 현재 색을 반환합니다 */
	static ERSBarrierColor GetAlternatingColor(ERSBarrierColor StartingColor, int32 Index);

	/** 방어막 중심이 안전 영역에 포함되는지 수평 거리로 검사합니다 */
	static bool IsLocationInsideSafeZone(const FVector& Location, const FVector& ZoneCenter, float SafeRadius);

protected:
	/** 시작 위치와 첫 색을 확정하고 방어막과 안광 예고를 시작합니다 */
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	/** 모든 종료 경로에서 이벤트 대기와 방어막 표현을 정리합니다 */
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

#if WITH_EDITOR
	/** 공간, Montage Notify, Niagara와 피해 설정이 실행 계약에 맞는지 검사합니다 */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	struct FRSBarrierZoneRuntimeState
	{
		FVector Center = FVector::ZeroVector;
		ERSBarrierColor Color = ERSBarrierColor::Red;
		TWeakObjectPtr<UNiagaraComponent> Presentation;
	};

	/** 고정된 시작 위치를 기준으로 판정에 사용할 네 방어막 상태를 생성합니다 */
	void CreateBarrierField();

	/** 생성된 네 방어막 상태에 대응하는 반복 Niagara를 생성합니다 */
	bool CreateBarrierPresentations();

	/** 방어막 하나의 반복 Niagara를 생성합니다 */
	UNiagaraComponent* CreateBarrierPresentation(const FRSBarrierZoneRuntimeState& Zone);

	/** 남아 있는 방어막 표현을 모두 제거합니다 */
	void DestroyBarrierZones();

	/** 현재 색과 일치하는 방어막 안에 플레이어가 있는지 검사합니다 */
	bool IsPlayerInsideCurrentSafeZone(const AActor& PlayerActor) const;

	/** 안광 이벤트 대기를 먼저 열고 예고 Montage를 재생합니다 */
	void StartPreviewMontage();

	/** 현재 공격의 이벤트 대기를 먼저 열고 공격 Montage를 재생합니다 */
	void StartAttackMontage();

	/** 현재 Montage용 Gameplay Event Task를 끝냅니다 */
	void EndGameplayEventTasks();

	/** 현재 색의 안광 또는 검 Niagara를 정의에 따라 생성합니다 */
	UNiagaraComponent* SpawnNiagaraFromDefinition(const FRSNiagaraSpawnDefinition& Definition, const FTransform& WorldTransform, bool bAutoDestroy) const;

	/** 안광 예고 이벤트를 현재 예고 색의 Niagara로 변환합니다 */
	UFUNCTION()
	void HandleEyeNiagaraEvent(FGameplayEventData Payload);

	/** 공격 검 점멸 이벤트를 현재 공격 색의 Niagara로 변환합니다 */
	UFUNCTION()
	void HandleSwordNiagaraEvent(FGameplayEventData Payload);

	/** 현재 공격의 판정 이벤트를 처리합니다 */
	UFUNCTION()
	void HandleHitCheckEvent(FGameplayEventData Payload);

	/** 안광 예고가 정상 완료되면 이벤트 개수를 확인하고 첫 공격을 시작합니다 */
	UFUNCTION()
	void HandlePreviewMontageCompleted();

	/** 공격이 정상 완료되면 계약을 확인하고 다음 공격 또는 패턴 종료로 진행합니다 */
	UFUNCTION()
	void HandleAttackMontageCompleted();

	/** Montage가 중단되거나 재생에 실패하면 패턴을 취소합니다 */
	UFUNCTION()
	void HandleMontageCancelled();

	/** 현재 공격 색으로 고정 범위를 판정하고 안전 영역 밖 플레이어에게 피해를 적용합니다 */
	void ExecuteCurrentHitCheck();

	/** 설정 오류 또는 런타임 이벤트 계약 위반을 기록하고 패턴을 취소합니다 */
	void CancelForInvalidRuntime(const TCHAR* Reason);

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Barrier Memory|Animation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimMontage> RoarMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Barrier Memory|Animation", meta = (AllowPrivateAccess = "true", ClampMin = "0.01", UIMin = "0.01"))
	float RoarMontagePlayRate = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Barrier Memory|Animation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimMontage> AttackMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Barrier Memory|Animation", meta = (AllowPrivateAccess = "true", ClampMin = "0.01", UIMin = "0.01"))
	float AttackMontagePlayRate = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Barrier Memory|Presentation", meta = (AllowPrivateAccess = "true"))
	FRSNiagaraSpawnDefinition RedEyeNiagara;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Barrier Memory|Presentation", meta = (AllowPrivateAccess = "true"))
	FRSNiagaraSpawnDefinition YellowEyeNiagara;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Barrier Memory|Presentation", meta = (AllowPrivateAccess = "true"))
	FRSNiagaraSpawnDefinition RedSwordNiagara;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Barrier Memory|Presentation", meta = (AllowPrivateAccess = "true"))
	FRSNiagaraSpawnDefinition YellowSwordNiagara;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Barrier Memory|Barrier", meta = (AllowPrivateAccess = "true"))
	FRSBarrierFieldDefinition BarrierField;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Barrier Memory|Hit", meta = (AllowPrivateAccess = "true"))
	FRSCombatShape AttackShape;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Barrier Memory|Hit", meta = (AllowPrivateAccess = "true"))
	TEnumAsByte<ECollisionChannel> TargetChannel = ECollisionChannel::ECC_GameTraceChannel1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Barrier Memory|Hit", meta = (AllowPrivateAccess = "true"))
	FScalableFloat Damage = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Barrier Memory|Hit", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> PresentationEventTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> HitCheckEventTask;

	TArray<FRSBarrierZoneRuntimeState> BarrierZones;

	UPROPERTY(Transient)
	FTransform PatternCenterTransform = FTransform::Identity;

	UPROPERTY(Transient)
	ERSBarrierColor StartingColor = ERSBarrierColor::Red;

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
