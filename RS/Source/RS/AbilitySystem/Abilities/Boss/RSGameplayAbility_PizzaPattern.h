#pragma once

#include "CoreMinimal.h"
#include "Combat/RSCombatFunctionLibrary.h"
#include "Components/RSAttackTelegraphComponent.h"
#include "Engine/EngineTypes.h"
#include "RSBaseGameplayAbility_BossPattern.h"
#include "RSGameplayAbility_PizzaPattern.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitDelay;
class UAnimMontage;
class UGameplayEffect;
class UNiagaraSystem;
class USoundBase;
class URSAbilityTask_WaitTelegraphFill;

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

	/** 패턴 중심에서 조각 외곽까지의 거리입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Pizza Pattern|Hit", meta = (ClampMin = "0.01", UIMin = "0.01", ForceUnits = "cm"))
	float OuterRadius = 0.0f;

	/** Ability 활성화 뒤 첫 Telegraph가 나타날 때까지의 독립 지연입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Pizza Pattern|Timing", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "s"))
	float AttackStartDelay = 0.0f;

	/** 각 Telegraph가 나타난 뒤 폭발할 때까지의 시간입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Pizza Pattern|Timing", meta = (ClampMin = "0.01", UIMin = "0.01", ForceUnits = "s"))
	float TelegraphDuration = 0.0f;

	/** Telegraph의 Fill 완료와 표시 소멸이 폭발보다 각각 얼마나 빠른지입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Pizza Pattern|Timing")
	FRSTelegraphLeadTime TelegraphLeadTime;

	/** 폭발 하나가 대상에게 가할 피해량입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Pizza Pattern|Hit")
	FScalableFloat Damage = 0.0f;

	/** 판정에 걸린 대상에게 요청할 피격 반응입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Pizza Pattern|Reaction")
	FRSHitReactionDefinition Reaction;

	/** 런타임 계산에 사용할 수 있는 값인지 검사합니다 */
	bool IsDataValid(FString* OutValidationError = nullptr) const;
};

/** 보스를 중심으로 고정한 A/B 피자 조각을 설정 횟수만큼 교대로 폭발시키는 Ability입니다 */
UCLASS(Abstract, Blueprintable)
class RS_API URSGameplayAbility_PizzaPattern : public URSBaseGameplayAbility_BossPattern
{
	GENERATED_BODY()

public:
	URSGameplayAbility_PizzaPattern();

protected:
	/** Montage를 한 번 요청하고 독립 지연 뒤 첫 A Telegraph를 시작합니다 */
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	/** 종료 경로와 관계없이 현재 Telegraph, 대기 Task와 애니메이션 상태를 정리합니다 */
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

#if WITH_EDITOR
	/** 조각 수, 폭발 횟수, 시간, 피해와 필수 GameplayEffect 설정을 검사합니다 */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	/** 설정된 Montage를 한 번 요청하며 실패와 종료는 공격 수명에 영향을 주지 않습니다 */
	void RequestAttackMontage();

	/** 공격 시작 지연이 끝나면 패턴 Transform을 확정하고 첫 Telegraph를 만듭니다 */
	UFUNCTION()
	void HandleAttackStartDelayFinished();

	/** 보스의 현재 Capsule 바닥과 Forward를 월드 고정 Transform으로 캡처합니다 */
	bool TryCaptureLockedPatternTransform();

	/** 현재 폭발 번호의 조각 Telegraph를 모두 표시하고 Fill Task를 시작합니다 */
	void BeginExplosionTelegraph();

	/** 현재 폭발의 모든 조각 Telegraph Fill을 같은 값으로 갱신합니다 */
	UFUNCTION()
	void HandleTelegraphFillUpdated(float Progress);

	/** Fill을 1로 확정한 뒤 현재 그룹을 폭발시키고 다음 그룹 또는 정상 종료로 진행합니다 */
	UFUNCTION()
	void HandleTelegraphFillFinished();

	/** 현재 폭발 그룹에 속하는 대상에게 피해와 반응을 한 번씩 요청합니다 */
	bool ExecuteCurrentExplosion();

	/** 현재 폭발의 선택적 Niagara와 Sound를 재생합니다 */
	void PlayExplosionPresentation() const;

	/** 현재 Ability가 만든 Telegraph Handle만 즉시 회수합니다 */
	void HideActiveTelegraphs();

	/** 현재 Ability가 만든 Telegraph의 Fill을 모두 갱신합니다 */
	bool SetActiveTelegraphFill(float Fill) const;

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

	/** 각 조각 위치에서 재생할 선택적 폭발 Niagara입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Pizza Pattern|Presentation")
	TObjectPtr<UNiagaraSystem> ExplosionNiagara;

	/** 폭발마다 패턴 중심에서 한 번 재생할 선택적 Sound입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Pizza Pattern|Presentation")
	TObjectPtr<USoundBase> ExplosionSound;

private:
	/** 공격 시작 시점에 한 번 캡처해 모든 Telegraph와 HitCheck가 공유하는 Transform입니다 */
	FTransform LockedPatternTransform = FTransform::Identity;

	/** 현재 폭발 그룹의 각 조각 Transform이며 표시와 폭발 연출이 함께 사용합니다 */
	TArray<FTransform> ActiveSliceTransforms;

	/** 현재 폭발 그룹에 표시한 Telegraph Handle입니다 */
	TArray<int32> ActiveTelegraphHandles;

	/** 같은 폭발에서 여러 후보로 수집된 Actor의 피해 중복을 막습니다 */
	TSet<TWeakObjectPtr<AActor>> HitActors;

	/** 0부터 시작하는 현재 폭발 번호이며 짝수는 A, 홀수는 B입니다 */
	int32 CurrentExplosionIndex = 0;

	/** Montage 재생 요청의 Task이며 완료·실패·중단 콜백은 공격 흐름에 연결하지 않습니다 */
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	/** 첫 Telegraph 시작까지 기다리는 Task입니다 */
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitDelay> AttackStartDelayTask;

	/** 현재 Telegraph의 Fill과 폭발 시각을 함께 소유하는 Task입니다 */
	UPROPERTY(Transient)
	TObjectPtr<URSAbilityTask_WaitTelegraphFill> TelegraphFillTask;

	/** EndAbility 정리 중 Task Callback 재진입을 막습니다 */
	bool bIsCleaningUp = false;
};
