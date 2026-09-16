#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "RSBaseGameplayAbility_BossPattern.h"
#include "RSPizzaMemoryPatternDefinition.h"
#include "RSGameplayAbility_PizzaMemoryPattern.generated.h"

class UAnimMontage;
class UAbilityTask_WaitDelay;
class UGameplayEffect;

/** 위험 조각의 순서를 기억한 뒤 같은 순서로 회피하는 피자 패턴 Ability입니다 */
UCLASS(Abstract, Blueprintable)
class RS_API URSGameplayAbility_PizzaMemoryPattern : public URSBaseGameplayAbility_BossPattern
{
	GENERATED_BODY()

public:
	URSGameplayAbility_PizzaMemoryPattern();

protected:
	/** Montage를 요청하고 독립 지연 뒤 패턴 Transform과 안전지대 후보 하나를 확정합니다 */
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	/** 종료 경로와 관계없이 공격 시작 대기와 이번 실행에 고정한 상태를 정리합니다 */
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

#if WITH_EDITOR
	/** 안전지대 후보 목록이 런타임에서 선택 가능한 형태인지 검사합니다 */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	/** 설정된 Montage를 한 번 요청하며 재생 실패와 중단은 패턴 준비를 취소하지 않습니다 */
	void RequestAttackMontage();

	/** 공격 시작 지연이 끝나면 패턴 Transform과 안전지대 순서 후보를 한 번 확정합니다 */
	UFUNCTION()
	void HandleAttackStartDelayFinished();

	/** 보스의 현재 Capsule 바닥과 Forward를 월드 고정 Transform으로 캡처합니다 */
	bool TryCaptureLockedPatternTransform();

	/** 후보 목록에서 같은 확률로 하나를 선택해 이번 실행의 단일 배열로 복사합니다 */
	bool TrySelectSafeZoneSequence();

	/** 현재 암기 항목의 위험 조각 6개를 완성된 Fill로 표시하고 유지 Task를 시작합니다 */
	bool BeginCurrentMemoryCue();

	/** 현재 암기 표시 시간이 끝나면 위험 조각을 숨기고 다음 간격 또는 암기 종료로 진행합니다 */
	UFUNCTION()
	void HandleMemoryCueDurationFinished();

	/** 암기 항목 사이의 무표시 간격이 끝나면 다음 위험 조각 표시를 시작합니다 */
	UFUNCTION()
	void HandleMemoryCueGapFinished();

	/** 마지막 암기 표시 뒤 모든 Telegraph가 사라진 상태로 회상 대기를 시작합니다 */
	void BeginRecallDelay();

	/** 회상 대기가 끝나면 이후 첫 폭발이 시작될 경계로 진행합니다 */
	UFUNCTION()
	void HandleRecallDelayFinished();

	/** 현재 폭발을 실행하고 다음 간격 또는 패턴 타임라인 완료로 진행합니다 */
	void RunCurrentExplosion();

	/** 폭발 사이의 공통 간격이 끝나면 선택 배열의 다음 폭발을 실행합니다 */
	UFUNCTION()
	void HandleExplosionIntervalFinished();

	/** 현재 안전쌍의 위험 영역에 있는 대상에게 피해와 반응을 Actor당 한 번 적용합니다 */
	bool ExecuteCurrentExplosion();

	/** 현재 위험 조각 6개의 공간 데이터로 폭발 연출을 한 번 재생합니다 */
	void PlayCurrentExplosionPresentation();

	/** 현재 Ability가 만든 암기 Telegraph Handle만 즉시 회수합니다 */
	void HideActiveMemoryCue();

protected:
	/** 에디터에서 설정할 안전지대 순서 후보입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Pizza Memory Pattern")
	FRSPizzaMemoryPatternDefinition PizzaMemoryPatternDefinition;

	/** Ability 활성화 시 한 번만 재생을 요청할 선택적 Montage입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Pizza Memory Pattern|Animation")
	TObjectPtr<UAnimMontage> AttackMontage;

	/** 위험 조각 판정에 걸린 대상에게 적용할 필수 대미지 GameplayEffect입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Pizza Memory Pattern|Hit")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	/** 보스 공격이 노릴 PlayerHurtBox Trace Channel입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Pizza Memory Pattern|Hit")
	TEnumAsByte<ECollisionChannel> TargetChannel = ECollisionChannel::ECC_GameTraceChannel1;

private:
	/** 공격 시작 지연 뒤 한 번 캡처해 이후 암기 표시와 폭발이 공유할 Transform입니다 */
	FTransform LockedPatternTransform = FTransform::Identity;

	/** 활성화마다 후보 하나를 복사해 이후 암기와 폭발이 함께 순회할 안전지대 배열입니다 */
	TArray<ERSPizzaMemorySafePair> ActiveSafePairs;

	/** 현재 암기 항목의 위험 조각 Transform이며 표시와 이후 폭발 연출이 공유할 공간 데이터입니다 */
	TArray<FTransform> ActiveDangerousSliceTransforms;

	/** 현재 암기 항목에 표시한 위험 조각 Telegraph Handle입니다 */
	TArray<int32> ActiveMemoryCueHandles;

	/** 암기 표시부터 마지막 폭발까지 선택 배열의 인덱스와 실행 단계를 관리합니다 */
	FRSPizzaCueTimeline Timeline;

	/** 같은 폭발에서 여러 HurtBox로 수집된 Actor의 피해와 반응 중복을 막습니다 */
	TSet<TWeakObjectPtr<AActor>> HitActors;

	/** 패턴 Transform과 후보를 확정하기 전까지 기다리는 Task입니다 */
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitDelay> AttackStartDelayTask;

	/** 현재 위험 조각 표시를 MemoryCueDuration 동안 유지하는 Task입니다 */
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitDelay> MemoryCueDurationTask;

	/** 마지막 항목을 제외한 암기 표시 사이를 MemoryCueGap 동안 비워 두는 Task입니다 */
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitDelay> MemoryCueGapTask;

	/** 마지막 암기 표시 뒤 첫 폭발 전까지 모든 표시를 비워 두는 Task입니다 */
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitDelay> RecallDelayTask;

	/** 첫 폭발 이후 선택 배열의 다음 폭발까지 기다리는 Task입니다 */
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitDelay> ExplosionIntervalTask;

	/** EndAbility 정리 중 Task Callback 재진입을 막습니다 */
	bool bIsCleaningUp = false;
};
