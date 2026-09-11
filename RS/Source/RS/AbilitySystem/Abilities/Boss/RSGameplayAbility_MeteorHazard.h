#pragma once

#include "CoreMinimal.h"
#include "RSBaseGameplayAbility_BossPattern.h"
#include "RSGameplayAbility_MeteorHazard.generated.h"

class ARSBossMeteorHazard;
class UAnimMontage;

/** 포효 뒤 플레이어를 추적하는 운석 예고 하나를 생성하는 특수 패턴입니다 */
UCLASS(Abstract, Blueprintable)
class RS_API URSGameplayAbility_MeteorHazard : public URSBaseGameplayAbility_BossPattern
{
	GENERATED_BODY()

public:
	URSGameplayAbility_MeteorHazard();

	/** 장판 피해가 별도 Actor와 Ability에서 발생하므로 이 패턴 자체에는 파훼 판정을 두지 않습니다 */
	virtual bool HasGimmickBreakCondition() const override { return false; }

protected:
	/** 현재 보스 TargetActor를 캡처하고 시작 지연 뒤 운석 Actor 하나를 생성합니다 */
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	/** 취소에서는 진행도가 100%를 지났더라도 이번 실행이 만든 장판을 함께 제거합니다 */
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

#if WITH_EDITOR
	/** 운석 Class와 시작 지연을 검사합니다 */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	/** 활성화 순간의 유효한 보스 TargetActor를 저장합니다 */
	bool CaptureTargetActor(const FGameplayAbilityActorInfo* ActorInfo);

	/** 시작 지연이 끝나면 운석 Actor를 생성합니다 */
	UFUNCTION()
	void HandleStartDelayFinished();

	/** 운석 예고가 장판으로 전환되면 정상 종료하고 조기 정리되면 취소 종료합니다 */
	void HandlePreparationFinished(bool bInHazardActivated);

	/** 포효 Montage의 완료, 실패 또는 중단을 표현 종료로 처리합니다 */
	UFUNCTION()
	void HandleRoarMontageFinished();

	/** 장판 활성화와 포효 종료가 모두 끝났으면 Ability를 정상 종료합니다 */
	void TryFinishAbility();

private:
	/** 운석 예고 전에 재생할 선택적 포효 Montage입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Meteor Hazard|Presentation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimMontage> RoarMontage;

	/** Ability 활성화 후 Telegraph를 시작하기까지의 시간입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Meteor Hazard|Timing", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0", ForceUnits = "s"))
	float AttackStartDelay = 0.5f;

	/** 한 번의 패턴에서 생성할 운석 장판 Actor Class입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Meteor Hazard", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<ARSBossMeteorHazard> MeteorHazardClass;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> CapturedTargetActor;

	UPROPERTY(Transient)
	TObjectPtr<ARSBossMeteorHazard> ActiveMeteorHazard;

	FDelegateHandle PreparationFinishedDelegateHandle;
	bool bHazardActivated = false;
	bool bRoarMontageFinished = false;
};
