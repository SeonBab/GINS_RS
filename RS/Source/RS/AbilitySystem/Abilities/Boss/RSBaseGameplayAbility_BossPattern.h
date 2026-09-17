// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Combat/RSCombatFunctionLibrary.h"
#include "Containers/ArrayView.h"
#include "RSBaseGameplayAbility.h"
#include "RSNiagaraSpawnDefinition.h"
#include "RSBaseGameplayAbility_BossPattern.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAnimMontage;
class UNiagaraComponent;
class USoundBase;
class URSAbilityTask_BossFacing;
struct FRSBossFacingRequest;

/** 패턴 연출 Niagara를 공격 판정 범위에 어떻게 배치할지 정합니다 */
UENUM(BlueprintType)
enum class ERSBossPatternNiagaraPlacement : uint8
{
	AbilityDefined	UMETA(DisplayName = "Ability Defined",	ToolTip = "채우지 않고 어빌리티가 정한 지점에서 재생합니다. 위치는 패턴마다 고정이라 여기서 바꿀 수 없습니다."),
	FillHitShape	UMETA(DisplayName = "Fill Hit Shape",	ToolTip = "공격 판정 범위를 격자로 채워 재생합니다. Niagara Fill Spacing으로 밀도를 조절합니다.")
};

/**
 * 패턴이 재생할 Niagara 하나와 그 배치 방식입니다
 * 한 연출이 바닥, 충격파, 섬광처럼 성격이 다른 Niagara를 겹쳐 쓸 수 있으므로 배치도 항목마다 따로 정합니다
 */
USTRUCT(BlueprintType)
struct FRSBossPatternNiagaraEntry
{
	GENERATED_BODY()

	/** 생성할 Niagara와 그 생성 기준입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Boss Pattern|Presentation")
	FRSNiagaraSpawnDefinition Niagara;

	/**
	 * 이 Niagara를 어빌리티가 정한 지점에 둘지 공격 판정 범위에 채울지 정합니다
	 * 판정 범위를 채우려면 패턴이 자기 판정 형상을 넘겨야 하므로, 형상을 넘기지 않는 패턴에서는 채우기를 골라도 지점 하나로 재생합니다
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Boss Pattern|Presentation")
	ERSBossPatternNiagaraPlacement Placement = ERSBossPatternNiagaraPlacement::AbilityDefined;

	/**
	 * 공격 범위를 이 Niagara로 채울 때 사용할 격자 간격이며 좁을수록 빼곡해집니다
	 * 범위보다 넓게 두면 범위를 대표하는 지점에 하나만 생성합니다
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Boss Pattern|Presentation", meta = (ClampMin = "1.0", UIMin = "10.0", ForceUnits = "cm", EditCondition = "Placement == ERSBossPatternNiagaraPlacement::FillHitShape", EditConditionHides))
	float FillSpacing = 150.0f;
};

/**
 * 패턴이 판정과 무관하게 재생하는 연출이며 항목은 전부 선택적입니다
 * 적중한 대상이 아니라 패턴이 정한 위치를 기준으로 재생하므로 빗나가도 그대로 보입니다
 * 이 점이 적중한 대상마다 재생하는 FRSHitFeedbackDefinition과 다르며, 그래서 보스 패턴은 그 정의를 쓰지 않습니다
 */
USTRUCT(BlueprintType)
struct FRSBossPatternPresentation
{
	GENERATED_BODY()

	/**
	 * 이 연출이 재생할 Niagara 목록이며 항목마다 배치 방식과 격자 간격을 따로 정합니다
	 * 항목이 여러 개여도 Sound와 카메라 셰이크는 한 번의 연출에 한 번만 재생합니다
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Boss Pattern|Presentation")
	TArray<FRSBossPatternNiagaraEntry> Niagaras;

	/** 한 번의 연출에서 한 번만 재생할 Sound입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Boss Pattern|Presentation")
	TObjectPtr<USoundBase> Sound;

	/** 한 번의 연출에서 한 번만 흔들 카메라 셰이크입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Boss Pattern|Presentation")
	FRSCameraShakeDefinition CameraShake;
};

/**
 * 보스가 페이즈에서 사용하는 공격 패턴의 공통 부모입니다
 * 어느 패턴이든 페이즈의 메인 기믹으로 지정될 수 있도록 파훼 판정과 결과 보고를 담당합니다
 * 이번 실행이 실제로 기믹인지는 페이즈 상태를 아는 URSBossPhaseComponent가 정하므로 패턴은 알지 않습니다
 */
UCLASS(Abstract)
class RS_API URSBaseGameplayAbility_BossPattern : public URSBaseGameplayAbility
{
	GENERATED_BODY()

public:
	/**
	 * 이 패턴에 파훼 판정이 있는지 반환합니다
	 * 판정이 없는 패턴을 메인 기믹으로 지정하면 그 페이즈가 로그 없이 항상 실패하므로 Data Validation이 이 값을 검사합니다
	 */
	virtual bool HasGimmickBreakCondition() const { return true; }

protected:
	/**
	 * 이전 실행의 상태를 되돌린 뒤 선딜만큼 기다렸다가 BeginPatternTimeline()을 부릅니다
	 * 패턴이 이 함수를 재정의하지 않으므로 Super 호출을 빠뜨려 부모의 초기화가 통째로 건너뛰어지는 일이 생기지 않습니다
	 */
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	/**
	 * 선딜이 끝나 이 패턴의 예고와 판정 타임라인을 시작해도 되는 시점입니다
	 * 활성화 인자는 따로 받지 않고 CurrentSpecHandle, CurrentActorInfo와 CurrentActivationInfo를 사용합니다
	 */
	virtual void BeginPatternTimeline() PURE_VIRTUAL(URSBaseGameplayAbility_BossPattern::BeginPatternTimeline, );

	/** 파훼 판정 결과를 페이즈 컴포넌트에 보고합니다 */
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/**
	 * 판정에 걸린 대상을 기록한 뒤 피해를 적용합니다
	 * 패턴마다 판정 방식이 달라도 피해 적용은 모두 이 지점을 지나므로 여기서 한 번만 셉니다
	 */
	virtual void ApplyDamageToTarget(AActor* TargetActor, TSubclassOf<UGameplayEffect> DamageEffectClass, float DamageAmount) override;

	/**
	 * 이번 실행에서 판정에 걸린 참가자가 한 명도 없으면 파훼로 봅니다
	 * 피격 여부로 판정할 수 없는 패턴은 이 함수를 재정의해 자신의 조건을 사용합니다
	 */
	virtual bool IsGimmickBroken() const { return !bAnyTargetHit; }

	/**
	 * 패턴 Montage 재생을 요청하고 그 Montage가 끝날 때까지 정상 종료를 막습니다
	 * 반환한 Task에 패턴이 자기 Callback을 더 붙일 수 있으며 Montage가 없거나 Task를 만들지 못하면 nullptr을 돌려줍니다
	 * 종료 시 Task 정리와 Montage가 남긴 상태 태그 회수도 이 부모가 맡으므로 패턴이 따로 처리하지 않습니다
	 */
	UAbilityTask_PlayMontageAndWait* PlayPatternMontage(UAnimMontage* Montage, float PlayRate = 1.0f);

	/**
	 * 패턴의 공격 타임라인이 끝나 정상 종료해도 되는 시점에 호출합니다
	 * PlayPatternMontage()로 요청한 Montage가 아직 남아 있으면 마지막 하나가 끝나는 순간으로 정상 종료를 미룹니다
	 * 취소 경로는 이 함수를 쓰지 않고 지금처럼 EndAbility()를 직접 불러 즉시 끝냅니다
	 *
	 * 이 게이트는 **이미 재생을 요청한** Montage만 셉니다
	 * 그래서 나중에 재생할 Montage가 더 있는 패턴은 그 요청을 먼저 하고 이 함수를 호출해야 합니다
	 * 순서가 뒤바뀌면 그 순간 남은 Montage가 0개라 어빌리티가 먼저 끝납니다
	 */
	void FinishPatternWhenMontageEnds();

	/** 기존 Facing을 정리하고 이 패턴이 사용할 새 보스 Facing Task를 생성합니다 */
	URSAbilityTask_BossFacing* CreateBossFacingTask(const FRSBossFacingRequest& Request);

	/** 현재 패턴이 소유한 Facing Task와 회전 설정을 즉시 정리합니다 */
	void EndActiveBossFacing();

	/**
	 * 이 패턴의 연출을 재생하며 적중 여부는 보지 않습니다
	 * Niagara는 넘긴 위치마다 하나씩, Sound와 카메라 셰이크는 호출마다 한 번 재생합니다
	 * 어느 위치에서 몇 번 재생할지는 패턴마다 다르므로 부모가 정하지 않고 호출자가 위치를 넘깁니다
	 */
	void PlayPatternPresentation(const TArray<FTransform>& NiagaraTransforms, const FVector& SoundLocation) const;

	/** 연출 지점이 하나인 패턴이 사용하는 간편 형태이며 Sound도 같은 위치에서 재생합니다 */
	void PlayPatternPresentation(const FTransform& PresentationTransform) const;

	/**
	 * 판정 형상을 함께 넘겨 항목마다 배치 방식에 따라 연출 지점을 정하는 형태입니다
	 * FillHitShape 항목은 넘긴 형상을 격자로 채우고, AbilityDefined 항목은 넘긴 Transform을 그대로 씁니다
	 * 한 번의 연출이 여러 영역을 덮는 패턴도 영역을 한 번에 넘기면 Sound와 카메라 셰이크는 그대로 한 번만 재생합니다
	 * 채우기에 실패하면 그 영역의 연출이 사라지지 않도록 넘긴 Transform으로 되돌립니다
	 */
	void PlayPatternPresentation(const FRSCombatShape& HitShape, const TArray<FTransform>& ShapeTransforms, const FVector& SoundLocation) const;

	/** 판정 영역이 하나인 패턴이 사용하는 간편 형태이며 Sound도 그 영역의 기준 위치에서 재생합니다 */
	void PlayPatternPresentation(const FRSCombatShape& HitShape, const FTransform& ShapeTransform) const;

	/**
	 * 한 번의 판정이 같은 자리에 겹치는 서로 다른 형상 여러 개를 덮을 때 사용하는 형태입니다
	 * 형상마다 배치 방식을 따로 적용하지만 Sound와 카메라 셰이크는 한 번만 재생하므로 한 번의 공격이 여러 번으로 들리지 않습니다
	 */
	void PlayPatternPresentation(TArrayView<const FRSCombatShape> HitShapes, const FTransform& ShapeTransform) const;

#if WITH_EDITOR
	/**
	 * 이 패턴의 연출 설정이 넘길 판정 형상을 실제로 채울 수 있는지 검사합니다
	 * FillHitShape가 아니면 채우기를 하지 않으므로 아무것도 검사하지 않습니다
	 */
	EDataValidationResult ValidatePatternPresentation(const FRSCombatShape& HitShape, FDataValidationContext& Context) const;
#endif

	/**
	 * 정의가 지정한 방식으로 Niagara 하나를 생성합니다
	 * 소켓 기준 생성은 보스 Skeletal Mesh를 사용하며 소켓이 없으면 경고를 남기고 건너뜁니다
	 */
	UNiagaraComponent* SpawnNiagaraFromDefinition(const FRSNiagaraSpawnDefinition& Definition, const FTransform& WorldTransform, bool bAutoDestroy) const;

protected:
	/**
	 * 이 패턴이 자기 연출 시점에 재생할 Niagara, Sound와 카메라 셰이크입니다
	 * 비워 두면 아무것도 재생하지 않으므로 Data Validation은 이 값을 필수로 검사하지 않습니다
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Boss Pattern|Presentation")
	FRSBossPatternPresentation PatternPresentation;

	/**
	 * 활성화 직후 아무것도 하지 않고 서 있을 시간이며 예고와 연출도 이 뒤에 시작합니다
	 * 0이면 대기 없이 같은 프레임에 타임라인을 시작하므로 이 값을 두기 전과 동작이 같습니다
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Boss Pattern|Timing", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "s"))
	float WindupDuration = 0.0f;

	/**
	 * 타임라인과 Montage가 모두 끝난 뒤 서 있을 시간이며 기존 종료 시점에 더해집니다
	 * 취소로 끝나는 경우에는 기다리지 않고 즉시 종료합니다
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Boss Pattern|Timing", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "s"))
	float RecoveryDuration = 0.0f;

private:
	/**
	 * 연출 항목을 모두 재생하며 형상이 없으면 채우기 항목도 넘겨받은 지점에서 재생합니다
	 * 형상과 위치를 교차 순회하므로 형상 하나를 여러 지점에 두는 패턴과 한 지점에 형상 여러 개를 겹치는 패턴이 같은 경로를 씁니다
	 * 모든 공개 형태가 이 지점을 지나므로 Sound와 카메라 셰이크의 1회 재생을 여기서만 보장합니다
	 */
	void PlayPatternPresentationInternal(TArrayView<const FRSCombatShape> HitShapes, TArrayView<const FTransform> PresentationTransforms, const FVector& SoundLocation) const;

	/** 재생이 끝난 Montage 하나를 게이트에서 내리고 마지막 하나였으면 보류한 정상 종료를 진행합니다 */
	UFUNCTION()
	void HandlePatternMontageFinished();

	/** 선딜 대기가 끝나 패턴 타임라인을 시작합니다 */
	UFUNCTION()
	void HandleWindupFinished();

	/** 후딜 대기가 끝나 보류한 정상 종료를 진행합니다 */
	UFUNCTION()
	void HandleRecoveryFinished();

	/** 타임라인과 Montage가 모두 끝났을 때만 정상 종료합니다 */
	void TryFinishPattern();

	/** 이번 실행에서 판정에 걸린 대상이 있었는지 나타냅니다 */
	bool bAnyTargetHit = false;

	/** 패턴이 자기 타임라인을 끝내고 정상 종료를 요청했는지 나타냅니다 */
	bool bPatternTimelineFinished = false;

	/** 후딜을 기다리는 중인지 나타내며 같은 대기를 두 번 시작하지 않게 막습니다 */
	bool bIsInPatternRecovery = false;

	/** 재생을 요청했고 아직 끝나지 않은 Montage의 개수입니다 */
	int32 PendingPatternMontageCount = 0;

	/** 종료할 때 함께 정리할 이번 실행의 Montage Task입니다 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UAbilityTask_PlayMontageAndWait>> PatternMontageTasks;

	/** 종료할 때 상태 태그를 회수할 이번 실행의 Montage입니다 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UAnimMontage>> PlayedPatternMontages;

	/** 종료할 때 Focus와 회전 설정을 함께 복원할 현재 Facing Task입니다 */
	UPROPERTY(Transient)
	TObjectPtr<URSAbilityTask_BossFacing> ActiveBossFacingTask;
};
