// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Combat/RSCombatFunctionLibrary.h"
#include "Engine/EngineTypes.h"
#include "RSBaseGameplayAbility.h"
#include "RSGameplayAbility_ConcentricRings.generated.h"

/**
 * 한 번의 패턴에서 링이 공격될 순서이며 각 항목은 Rings 배열의 인덱스입니다
 * TArray를 직접 중첩할 수 없어 후보 목록을 만들기 위한 래퍼입니다
 */
USTRUCT(BlueprintType)
struct FRSRingAttackSequence
{
	GENERATED_BODY()

	/** 공격될 링 인덱스를 순서대로 나열하며 같은 링이 여러 번 등장할 수 있습니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Rings")
	TArray<int32> RingIndices;
};

/**
 * 보스를 중심으로 하는 동심원 링을 순서대로 예고한 뒤 같은 순서로 판정하는 패턴입니다
 * 플레이어는 반응이 아니라 예고에서 본 순서를 기억해 회피하므로 실행 단계에는 표시를 하지 않습니다
 *
 * 예고 시간이 애니메이션과 독립한 게임플레이 값이어야 하므로 판정 시점을 Montage Notify가 정하는
 * URSBaseGameplayAbility_Attack을 상속하지 않고 타이밍을 이 어빌리티가 직접 소유합니다
 */
UCLASS(Abstract, Blueprintable)
class RS_API URSGameplayAbility_ConcentricRings : public URSBaseGameplayAbility
{
	GENERATED_BODY()

public:
	/** 링 레이아웃과 타이밍의 개발용 기본값을 구성합니다 */
	URSGameplayAbility_ConcentricRings();

protected:
	/** 이번 실행의 시퀀스와 링 중심을 확정하고 첫 예고 스텝을 시작합니다 */
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

#if WITH_EDITOR
	/** 링 레이아웃과 시퀀스가 패턴이 성립하는 범위를 벗어나지 않았는지 검사합니다 */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	/**
	 * 현재 StepIndex의 스텝을 실행하고 다음 스텝을 예약합니다
	 * 단계를 따로 저장하지 않고 StepIndex에서 파생하므로 예고와 실행이 같은 시퀀스를 두 번 순회합니다
	 */
	void RunCurrentStep();

	/** 지정한 시간 뒤에 다음 스텝으로 넘어갈 대기 Task를 시작합니다 */
	void ScheduleNextStep(float DelaySeconds);

	/** 대기가 끝나면 StepIndex를 진행하고 다음 스텝을 실행합니다 */
	UFUNCTION()
	void HandleStepDelayFinished();

	/** 시퀀스의 한 항목이 가리키는 링 형상을 반환하며 인덱스가 잘못되면 nullptr입니다 */
	const FRSCombatShape* GetRingShape(int32 SequenceIndex) const;

	/** 예고 스텝입니다. 데미지 없이 링을 표시만 합니다 */
	void PreviewRing(const AActor& AvatarActor, int32 SequenceIndex);

	/** 실행 스텝입니다. 판정하고 피해와 반응을 적용합니다 */
	void StrikeRing(const AActor& AvatarActor, int32 SequenceIndex);

private:
	/** 보스를 중심으로 하는 링 목록이며 안쪽부터 반경 오름차순으로 나열합니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Rings", meta = (AllowPrivateAccess = "true"))
	TArray<FRSCombatShape> Rings;

	/** 활성화마다 하나를 고를 공격 순서 후보입니다. 후보가 하나면 고정 순서가 됩니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Rings", meta = (AllowPrivateAccess = "true"))
	TArray<FRSRingAttackSequence> SequencePool;

	/** 예고 한 스텝에서 링을 보여줄 시간입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Rings|Timing", meta = (AllowPrivateAccess = "true", ClampMin = "0.01", UIMin = "0.01", ForceUnits = "s"))
	float PreviewShowDuration = 0.5f;

	/** 예고 스텝 사이의 간격이며 스텝 경계를 읽을 수 있게 합니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Rings|Timing", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0", ForceUnits = "s"))
	float PreviewInterval = 0.2f;

	/** 마지막 예고가 끝나고 첫 공격이 시작될 때까지 플레이어가 자리를 잡을 시간입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Rings|Timing", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0", ForceUnits = "s"))
	float InterludeDuration = 1.0f;

	/** 실제 공격 사이의 간격이며 링 하나를 건널 시간을 결정합니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Rings|Timing", meta = (AllowPrivateAccess = "true", ClampMin = "0.01", UIMin = "0.01", ForceUnits = "s"))
	float StrikeInterval = 1.0f;

	/** 링 하나가 대상에게 가할 피해량이며 링마다 다르게 두지 않습니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Rings|Hit", meta = (AllowPrivateAccess = "true"))
	FScalableFloat Damage = 15.0f;

	/** 판정에 걸린 대상에게 적용할 대미지 GameplayEffect입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Rings|Hit", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	/**
	 * 이 패턴이 노릴 진영의 허트박스 Trace Channel입니다
	 * 보스 공격이므로 기본값은 PlayerHurtBox이며, 플레이어가 적을 때리는 채널과 반대입니다
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Rings|Hit", meta = (AllowPrivateAccess = "true"))
	TEnumAsByte<ECollisionChannel> TargetChannel = ECollisionChannel::ECC_GameTraceChannel1;

	/** 판정에 걸린 대상에게 요청할 피격 반응입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Rings|Reaction", meta = (AllowPrivateAccess = "true"))
	FRSHitReactionDefinition Reaction;

private:
	/** 이번 활성화에서 확정한 공격 순서이며 예고와 실행이 같은 배열을 읽습니다 */
	UPROPERTY(Transient)
	TArray<int32> ActiveSequence;

	/**
	 * 이번 활성화 시작 시점에 캡처한 링 중심이며 캡슐 중심이 아니라 발밑 높이입니다
	 * 예고 중 보스가 움직여도 예고한 자리와 판정 자리가 갈라지지 않도록 월드에 고정합니다
	 * 판정은 수평 거리만 쓰지만 표시는 이 높이에 그려지므로 지면에 맞춰야 경계가 어긋나 보이지 않습니다
	 */
	UPROPERTY(Transient)
	FTransform RingCenterTransform = FTransform::Identity;

	/**
	 * 진행 중인 스텝 번호입니다
	 * 시퀀스 길이보다 작으면 예고, 그 이상이면 실행이며 두 배에 도달하면 패턴이 끝납니다
	 */
	UPROPERTY(Transient)
	int32 StepIndex = 0;
};
