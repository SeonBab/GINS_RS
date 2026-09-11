// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Combat/RSCombatFunctionLibrary.h"
#include "RSBaseGameplayAbility.h"
#include "RSGameplayAbility_RandomFallingRocks.generated.h"

class UGameplayEffect;
class UNiagaraSystem;
class USoundBase;
class URSAbilityTask_PendingFallingRock;
class UAbilityTask_WaitDelay;

/** 랜덤 낙석의 생성 간격, 예약 시간과 판정 데이터를 묶습니다 */
USTRUCT(BlueprintType)
struct FRSRandomFallingRocksDefinition
{
	GENERATED_BODY()

	/** 보스 중심에서 낙석이 생성될 수 있는 최소 수평 거리입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Falling Rocks", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm"))
	float MinSpawnRadius = 200.0f;

	/** 보스 중심에서 낙석이 생성될 수 있는 최대 수평 거리입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Falling Rocks", meta = (ClampMin = "0.01", UIMin = "0.01", ForceUnits = "cm"))
	float MaxSpawnRadius = 1000.0f;

	/** 낙석 예약 사이의 간격이며 활성화 뒤 첫 예약 전에도 한 번 기다립니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Falling Rocks|Timing", meta = (ClampMin = "0.01", UIMin = "0.01", ForceUnits = "s"))
	float RockInterval = 1.0f;

	/** 위치를 예약한 뒤 판정이 발생할 때까지의 시간입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Falling Rocks|Timing", meta = (ClampMin = "0.01", UIMin = "0.01", ForceUnits = "s"))
	float ImpactDelay = 2.0f;

	/** Impact 전에 낙하 Niagara를 시작할 시간입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Falling Rocks|Timing", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "s"))
	float FallEffectLeadTime = 1.0f;

	/** Telegraph와 실제 판정이 함께 사용할 원형 Shape입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Falling Rocks|Hit")
	FRSCombatShape AttackShape;

	/** 낙석 하나가 대상에게 가할 피해량입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Falling Rocks|Hit")
	FScalableFloat Damage = 10.0f;

	/** 판정에 걸린 대상에게 요청할 피격 반응입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Falling Rocks|Reaction")
	FRSHitReactionDefinition Reaction;

	/** 런타임 계산에 사용할 수 있는 값인지 검사합니다 */
	bool IsDataValid(FString* OutValidationError = nullptr) const;
};

/** 보스 주변에 예약형 낙석을 반복 생성하는 전투 지속형 어빌리티입니다 */
UCLASS(Abstract, Blueprintable)
class RS_API URSGameplayAbility_RandomFallingRocks : public URSBaseGameplayAbility
{
	GENERATED_BODY()

public:
	URSGameplayAbility_RandomFallingRocks();

#if WITH_DEV_AUTOMATION_TESTS
	/** 자동 테스트가 현재 Pending Rock 수를 확인할 수 있게 합니다 */
	int32 GetPendingRockCountForTest() const { return PendingRockTasks.Num(); }

	/** 자동 테스트가 Interval 완료 뒤 예약 경로를 직접 실행할 수 있게 합니다 */
	void AdvanceRockIntervalForTest() { HandleRockIntervalFinished(); }

	/** 자동 테스트가 지정한 Pending Rock을 확인할 수 있게 합니다 */
	URSAbilityTask_PendingFallingRock* GetPendingRockForTest(int32 Index) const { return PendingRockTasks.IsValidIndex(Index) ? PendingRockTasks[Index] : nullptr; }
#endif

protected:
	/** 첫 Interval 대기를 시작하고 이후 같은 간격으로 낙석을 예약합니다 */
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	/** 새 예약을 막고 모든 Pending Rock과 재생 중인 낙하 Niagara를 정리합니다 */
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

#if WITH_EDITOR
	/** 낙석의 생성, 타이밍, 판정과 에셋 설정을 검사합니다 */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	/** 다음 낙석 예약까지의 대기를 시작합니다 */
	void ScheduleNextRock();

	/** Interval이 끝나면 낙석 하나를 예약하고 다음 Interval을 시작합니다 */
	UFUNCTION()
	void HandleRockIntervalFinished();

	/** 현재 보스 위치를 기준으로 낙석 하나를 예약합니다 */
	void ReserveRock();

	/** 예약된 낙석의 판정과 Impact 연출을 실행합니다 */
	UFUNCTION()
	void HandleRockImpact(FTransform ImpactTransform, URSAbilityTask_PendingFallingRock* PendingRockTask);

	/** Avatar의 발밑을 낙석 생성 영역 중심으로 반환합니다 */
	bool TryGetSpawnCenter(FVector& OutSpawnCenter) const;

protected:
	/** 낙석의 공간, 시간, 피해와 반응 설정입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Falling Rocks")
	FRSRandomFallingRocksDefinition FallingRocksDefinition;

	/** 이 패턴이 노릴 진영의 허트박스 Trace Channel입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Falling Rocks|Hit")
	TEnumAsByte<ECollisionChannel> TargetChannel = ECollisionChannel::ECC_GameTraceChannel1;

	/** 판정에 걸린 대상에게 적용할 대미지 GameplayEffect입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Falling Rocks|Hit")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	/** Impact 전에 재생할 선택적 낙하 Niagara입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Falling Rocks|Presentation")
	TObjectPtr<UNiagaraSystem> FallingRockNiagara;

	/** 판정 시점에 재생할 선택적 Impact Niagara입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Falling Rocks|Presentation")
	TObjectPtr<UNiagaraSystem> ImpactNiagara;

	/** 판정 시점에 재생할 Impact Sound이며 비어 있으면 소리를 재생하지 않습니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Falling Rocks|Presentation")
	TObjectPtr<USoundBase> ImpactSound;

	/** Telegraph가 표시되는 동안 사용할 불투명도입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Falling Rocks|Presentation", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float TelegraphOpacity = 1.0f;

private:
	/** 다음 낙석 예약을 기다리는 Task입니다 */
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitDelay> GenerationDelayTask;

	/** 아직 Impact하지 않은 낙석 Task입니다 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<URSAbilityTask_PendingFallingRock>> PendingRockTasks;

	/** 활성화마다 새 Seed를 사용하는 위치 생성기입니다 */
	FRandomStream RandomStream;
};
