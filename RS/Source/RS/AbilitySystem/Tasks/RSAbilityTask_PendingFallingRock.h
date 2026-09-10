#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "Combat/RSCombatFunctionLibrary.h"
#include "Components/RSAttackTelegraphComponent.h"
#include "RSAbilityTask_PendingFallingRock.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class URSAbilityTask_PendingFallingRock;
class URSAttackTelegraphComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRSPendingFallingRockImpactSignature, FTransform, ImpactTransform, URSAbilityTask_PendingFallingRock*, PendingRockTask);

/** 낙석 하나의 Telegraph, 낙하 연출과 Impact 시각을 함께 소유합니다 */
UCLASS()
class RS_API URSAbilityTask_PendingFallingRock : public UAbilityTask
{
	GENERATED_BODY()

public:
	/** 지정한 위치에서 낙석 하나를 예약하는 AbilityTask를 생성합니다 */
	static URSAbilityTask_PendingFallingRock* CreatePendingFallingRock(UGameplayAbility* OwningAbility, const FTransform& ImpactTransform, const FRSCombatShape& AttackShape, const FRSTelegraphPresentation& TelegraphPresentation, float FallEffectStartDelay, float ImpactDelay, UNiagaraSystem* FallingRockNiagara);

	virtual void Activate() override;

#if WITH_DEV_AUTOMATION_TESTS
	/** 자동 테스트가 같은 Impact 콜백을 반복 요청할 수 있게 합니다 */
	void TriggerImpactTwiceForTest()
	{
		HandleImpact();
		HandleImpact();
	}

	/** 자동 테스트 중 실제로 확정된 Impact 횟수를 반환합니다 */
	int32 GetCommittedImpactCountForTest() const { return CommittedImpactCountForTest; }
#endif

protected:
	/** Task가 끝날 때 Timer, Telegraph와 재생 중인 낙하 Niagara를 정리합니다 */
	virtual void OnDestroy(bool bInOwnerFinished) override;

public:
	/** 예약한 Impact 시각에 한 번 발생합니다 */
	UPROPERTY(BlueprintAssignable, Category = "RS|Falling Rocks")
	FRSPendingFallingRockImpactSignature OnImpact;

private:
	/** 낙하 Niagara를 시작합니다 */
	void HandleFallEffectStart();

	/** 표시와 낙하 Niagara를 회수하고 Impact를 전달합니다 */
	void HandleImpact();

	/** Task가 소유한 Timer를 모두 해제합니다 */
	void ClearTimers();

	/** Task가 소유한 Telegraph와 낙하 Niagara를 즉시 회수합니다 */
	void CleanupPresentation();

private:
	TWeakObjectPtr<URSAttackTelegraphComponent> TelegraphComp;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> FallingNiagaraComp;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraSystem> FallingRockNiagara;

	FTransform ImpactTransform = FTransform::Identity;
	FRSCombatShape AttackShape;
	FRSTelegraphPresentation TelegraphPresentation;
	float FallEffectStartDelay = 0.0f;
	float ImpactDelay = 0.0f;
	FTimerHandle FallEffectTimerHandle;
	FTimerHandle ImpactTimerHandle;
	int32 TelegraphHandle = INDEX_NONE;
	bool bImpactCommitted = false;

#if WITH_DEV_AUTOMATION_TESTS
	int32 CommittedImpactCountForTest = 0;
#endif
};
