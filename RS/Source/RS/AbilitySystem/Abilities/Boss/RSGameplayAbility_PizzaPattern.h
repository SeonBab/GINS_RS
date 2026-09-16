#pragma once

#include "CoreMinimal.h"
#include "Combat/RSCombatFunctionLibrary.h"
#include "Engine/EngineTypes.h"
#include "RSBaseGameplayAbility_BossPattern.h"
#include "RSPizzaCueTimeline.h"
#include "RSGameplayAbility_PizzaPattern.generated.h"

class UAbilityTask_WaitDelay;
class UAnimMontage;
class UGameplayEffect;

/** 피자 패턴의 공간, 횟수, 시간, 피해와 반응 설정입니다 */
USTRUCT(BlueprintType)
struct FRSPizzaPatternDefinition
{
	GENERATED_BODY()

	/** 한 번의 폭발에서 생성할 피자 조각 수입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Pizza Pattern", meta = (ClampMin = "2", UIMin = "2"))
	int32 SliceCount = 2;

	/** 한 활성화에서 A와 B를 교대로 실행할 총 폭발 횟수입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Pizza Pattern", meta = (ClampMin = "1", UIMin = "1"))
	int32 ExplosionCount = 1;

	/** 패턴 중심에서 조각 안쪽 경계까지의 거리이며 0이면 패턴 원점부터 판정합니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Pizza Pattern|Hit", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm"))
	float InnerRadius = 0.0f;

	/** 패턴 중심에서 조각 외곽까지의 거리입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Pizza Pattern|Hit", meta = (ClampMin = "0.01", UIMin = "0.01", ForceUnits = "cm"))
	float OuterRadius = 0.0f;

	/** Ability 활성화 뒤 첫 예고가 나타날 때까지의 독립 지연입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Pizza Pattern|Timing", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "s"))
	float AttackStartDelay = 0.0f;

	/** 한 폭발 그룹의 조각을 완성된 Fill 상태로 보여 줄 시간입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Pizza Pattern|Timing", meta = (ClampMin = "0.01", UIMin = "0.01", ForceUnits = "s"))
	float TelegraphCueDuration = 0.0f;

	/** 마지막 그룹을 제외한 예고 사이에 아무것도 보여 주지 않을 시간이며 1회형은 사용하지 않습니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Pizza Pattern|Timing", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "s"))
	float TelegraphCueGap = 0.0f;

	/** 마지막 예고를 지운 뒤 첫 폭발까지 플레이어가 자리를 잡을 시간입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Pizza Pattern|Timing", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "s"))
	float RecallDelay = 0.0f;

	/** 첫 폭발 이후 다음 그룹이 폭발할 때까지의 간격이며 1회형은 사용하지 않습니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Pizza Pattern|Timing", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "s"))
	float ExplosionInterval = 0.0f;

	/** 폭발 하나가 대상에게 가할 피해량입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Pizza Pattern|Hit")
	FScalableFloat Damage = 0.0f;

	/** 판정에 걸린 대상에게 요청할 피격 반응입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Pizza Pattern|Reaction")
	FRSHitReactionDefinition Reaction;

	/** 런타임 계산에 사용할 수 있는 값인지 검사합니다 */
	bool IsDataValid(FString* OutValidationError = nullptr) const;

	/** A와 B가 나누어 사용할 전체 가상 조각 수를 반환합니다 */
	int32 CalculateVirtualSliceCount() const;

	/** 가상 조각 하나의 각도를 반환하며 설정이 잘못되면 0입니다 */
	float CalculateSliceAngleDegrees() const;

	/**
	 * 예고, 판정과 연출이 함께 사용할 조각 하나의 형상을 만듭니다
	 * 설정이 성립하지 않아 판정할 면적이 남지 않으면 실패를 알립니다
	 */
	bool TryMakeSliceShape(FRSCombatShape& OutSliceShape) const;

	/** 공용 타임라인이 단계별 대기에 사용할 시간값을 만듭니다 */
	FRSPizzaCueTimings MakeCueTimings() const;

	/** 폭발 번호가 사용할 A 또는 B 조각의 월드 Transform을 계산합니다 */
	bool TryBuildExplosionSliceTransforms(const FTransform& LockedTransform, int32 ExplosionIndex, TArray<FTransform>& OutSliceTransforms) const;

	/** 대상 위치가 지정한 폭발의 A 또는 B 그룹에 속하는지 반환합니다 */
	bool IsLocationInExplosionGroup(const FTransform& LockedTransform, int32 ExplosionIndex, const FVector& TargetLocation) const;
};

/** 폭발할 A/B 피자 조각을 순서대로 모두 예고한 뒤 같은 순서로 폭발시키는 Ability입니다 */
UCLASS(Abstract, Blueprintable)
class RS_API URSGameplayAbility_PizzaPattern : public URSBaseGameplayAbility_BossPattern
{
	GENERATED_BODY()

public:
	URSGameplayAbility_PizzaPattern();

protected:
	/** Montage를 한 번 요청하고 독립 지연 뒤 첫 A 예고를 시작합니다 */
	virtual void BeginPatternTimeline() override;

	/** 종료 경로와 관계없이 현재 Telegraph, 대기 Task와 애니메이션 상태를 정리합니다 */
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

#if WITH_EDITOR
	/** 조각 수, 폭발 횟수, 시간, 피해와 필수 GameplayEffect 설정을 검사합니다 */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	/** 설정된 Montage를 한 번 요청하며 재생 실패와 중단은 공격 흐름을 취소하지 않습니다 */
	void RequestAttackMontage();

	/** 공격 시작 지연이 끝나면 패턴 Transform을 확정하고 첫 예고를 만듭니다 */
	UFUNCTION()
	void HandleAttackStartDelayFinished();

	/** 보스의 현재 Capsule 바닥과 Forward, 그리고 모든 경로가 공유할 조각 형상을 캡처합니다 */
	bool TryCaptureLockedPatternState();

	/** 현재 순서의 폭발 그룹을 완성된 Fill로 표시하고 유지 Task를 시작합니다 */
	bool BeginCurrentTelegraphCue();

	/** 현재 예고 시간이 끝나면 표시를 지우고 다음 간격 또는 예고 종료로 진행합니다 */
	UFUNCTION()
	void HandleTelegraphCueDurationFinished();

	/** 예고 사이의 무표시 간격이 끝나면 다음 그룹 표시를 시작합니다 */
	UFUNCTION()
	void HandleTelegraphCueGapFinished();

	/** 마지막 예고 뒤 모든 Telegraph가 사라진 상태로 폭발 전 대기를 시작합니다 */
	void BeginRecallDelay();

	/** 폭발 전 대기가 끝나면 첫 폭발로 진행합니다 */
	UFUNCTION()
	void HandleRecallDelayFinished();

	/** 현재 폭발을 실행하고 다음 간격 또는 패턴 타임라인 완료로 진행합니다 */
	void RunCurrentExplosion();

	/** 폭발 사이의 간격이 끝나면 다음 그룹의 폭발을 실행합니다 */
	UFUNCTION()
	void HandleExplosionIntervalFinished();

	/** 현재 폭발 그룹에 속하는 대상에게 피해와 반응을 한 번씩 요청합니다 */
	bool ExecuteCurrentExplosion();

	/** 현재 폭발 그룹의 조각 공간 데이터로 폭발 연출을 한 번 재생합니다 */
	void PlayCurrentExplosionPresentation();

	/** 현재 Ability가 만든 Telegraph Handle만 즉시 회수합니다 */
	void HideActiveTelegraphs();

protected:
	/** 피자 패턴의 공간, 횟수, 시간, 피해와 반응 설정입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Pizza Pattern")
	FRSPizzaPatternDefinition PizzaPatternDefinition;

	/** Ability 활성화 시 한 번만 재생을 요청할 선택적 Montage입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Pizza Pattern|Animation")
	TObjectPtr<UAnimMontage> AttackMontage;

	/** 판정에 걸린 대상에게 적용할 대미지 GameplayEffect입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Pizza Pattern|Hit")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	/** 보스 공격이 노릴 PlayerHurtBox Trace Channel입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Pizza Pattern|Hit")
	TEnumAsByte<ECollisionChannel> TargetChannel = ECollisionChannel::ECC_GameTraceChannel1;

private:
	/** 공격 시작 시점에 한 번 캡처해 모든 Telegraph와 HitCheck가 공유하는 Transform입니다 */
	FTransform LockedPatternTransform = FTransform::Identity;

	/** 공격 시작 시점에 보스 캡슐 하한까지 밀어 올려 캡처한 조각 형상이며 예고, 판정과 연출이 함께 읽습니다 */
	FRSCombatShape ActiveSliceShape;

	/** 현재 그룹의 각 조각 Transform이며 표시와 폭발 연출이 함께 사용합니다 */
	TArray<FTransform> ActiveSliceTransforms;

	/** 현재 예고 그룹에 표시한 Telegraph Handle입니다 */
	TArray<int32> ActiveTelegraphHandles;

	/** 첫 예고부터 마지막 폭발까지 그룹 번호와 실행 단계를 관리합니다 */
	FRSPizzaCueTimeline Timeline;

	/** 같은 폭발에서 여러 후보로 수집된 Actor의 피해 중복을 막습니다 */
	TSet<TWeakObjectPtr<AActor>> HitActors;

	/** 첫 예고 시작까지 기다리는 Task입니다 */
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitDelay> AttackStartDelayTask;

	/** 현재 예고 표시를 TelegraphCueDuration 동안 유지하는 Task입니다 */
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitDelay> TelegraphCueDurationTask;

	/** 마지막 그룹을 제외한 예고 사이를 TelegraphCueGap 동안 비워 두는 Task입니다 */
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitDelay> TelegraphCueGapTask;

	/** 마지막 예고 뒤 첫 폭발 전까지 모든 표시를 비워 두는 Task입니다 */
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitDelay> RecallDelayTask;

	/** 첫 폭발 이후 다음 폭발까지 기다리는 Task입니다 */
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitDelay> ExplosionIntervalTask;

	/** EndAbility 정리 중 Task Callback 재진입을 막습니다 */
	bool bIsCleaningUp = false;
};
