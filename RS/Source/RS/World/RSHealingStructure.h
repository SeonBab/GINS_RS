#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RSHealingStructure.generated.h"

class ARSPlayerCharacter;
class UGameplayEffect;
class USphereComponent;
class UStaticMeshComponent;

/**
 * 영역에 들어온 플레이어 캐릭터의 체력을 회복시키고 일정 시간이 지나면 다시 회복시킬 수 있게 되는 구조물입니다
 * 재충전이 끝난 시점에 이미 영역 안에 있는 대상도 바로 회복시키므로 회복을 받으려고 영역을 드나들 필요가 없습니다
 * 현재 게임은 싱글 플레이이므로 권한 판정과 복제를 두지 않습니다
 */
UCLASS()
class RS_API ARSHealingStructure : public AActor
{
	GENERATED_BODY()

public:
	/** 본체와 충전 표시 Mesh, 플레이어 캐릭터만 감지하는 영역을 구성합니다 */
	ARSHealingStructure();

	/** 지금 회복시킬 수 있는 상태인지 반환합니다 */
	UFUNCTION(BlueprintPure, Category = "RS|Healing")
	bool IsCharged() const { return bIsCharged; }

protected:
	/** 시작 충전 상태를 적용하고 영역 진입 감지를 시작합니다 */
	virtual void BeginPlay() override;

protected:
	/**
	 * 플레이어 캐릭터의 접근을 감지할 영역이며 반지름이 곧 회복이 닿는 거리입니다
	 * 반지름을 바꿔도 중심이 움직이지 않으므로 이 영역을 배치 기준으로 삼습니다
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Healing")
	TObjectPtr<USphereComponent> TriggerArea;

	/** 회복시킬 수 있는 상태에서만 보이는 Mesh이며 플레이어가 멀리서 사용 가능 여부를 판단하는 근거입니다 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Healing")
	TObjectPtr<UStaticMeshComponent> ChargedMesh;

private:
	/** 영역에 들어온 대상에게 회복을 시도합니다 */
	UFUNCTION()
	void HandleTriggerAreaBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/** 재충전이 끝나 다시 회복시킬 수 있게 되었을 때 호출됩니다 */
	void HandleRechargeCompleted();

	/**
	 * 충전을 채우고 영역 안의 대상을 회복시킬 시점을 예약합니다
	 * 시작 충전과 재충전 완료가 같은 규칙을 따르도록 두 경로가 이 함수를 지납니다
	 */
	void BecomeCharged();

	/** 충전 표시를 보여 줄 시간이 지난 뒤 영역 안의 대상을 회복시킵니다 */
	void HandleChargedHealDelayFinished();

	/**
	 * 대상이 회복이 필요한 플레이어 캐릭터이면 회복시키고 충전을 소모합니다
	 * 두 진입 경로가 이 함수만 호출하므로 회복 조건이 한 곳에만 있습니다
	 */
	bool TryHealTarget(AActor* TargetActor);

	/** 영역 안에서 회복이 필요한 대상을 찾아 회복시킵니다 */
	bool TryHealTargetInTriggerArea();

	/** 대상 ASC에 회복 GameplayEffect를 적용합니다 */
	bool TryApplyHealEffect(const ARSPlayerCharacter* TargetPlayerCharacter) const;

	/** 회복을 받을 수 있는 대상인지 반환합니다 */
	static bool CanReceiveHealing(const ARSPlayerCharacter* PlayerCharacter);

	/** 충전 상태를 바꾸고 충전 표시 Mesh의 가시성을 맞춥니다 */
	void SetCharged(bool bInCharged);

	/** 충전을 비우고 재충전을 예약합니다 */
	void StartRecharge();

private:
	/**
	 * 게임 시작 시 이미 회복시킬 수 있는 상태인지 나타냅니다
	 * 여러 개를 배치하면서 일부만 처음부터 쓸 수 있게 두는 구성을 위해 배치마다 정합니다
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RS|Healing", meta = (AllowPrivateAccess = "true"))
	bool bStartCharged = true;

	/** 회복시킨 뒤 다시 회복시킬 수 있게 되기까지의 시간입니다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RS|Healing", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1", Units = "s"))
	float RechargeSeconds = 30.0f;

	/**
	 * 한 번에 회복시킬 체력입니다
	 * 공용 회복 GameplayEffect에 값을 고정하지 않고 구조물마다 다른 값을 SetByCaller로 전달합니다
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RS|Healing", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float HealAmount = 30.0f;

	/**
	 * 충전이 찬 뒤 영역 안에 있던 대상을 회복시키기까지 기다리는 시간입니다
	 * 충전 표시가 나타난 프레임에 바로 회복하면 Mesh가 보이자마자 사라져 플레이어가 무슨 일이 있었는지 알 수 없습니다
	 * 걸어 들어와서 받는 회복은 Mesh가 이미 보이고 있었으므로 이 시간을 기다리지 않습니다
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RS|Healing", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float ChargedHealDelaySeconds = 1.0f;

	/** 대상에게 적용할 즉시 회복 GameplayEffect이며 SetByCaller.Healing으로 회복량을 받습니다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RS|Healing", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGameplayEffect> HealEffectClass;

private:
	/** 현재 회복시킬 수 있는 상태인지 나타냅니다 */
	bool bIsCharged = false;

	/** 다시 회복시킬 수 있게 되는 시점을 예약한 타이머입니다 */
	FTimerHandle RechargeTimerHandle;

	/** 충전이 찬 뒤 영역 안의 대상을 회복시킬 시점을 예약한 타이머입니다 */
	FTimerHandle ChargedHealDelayTimerHandle;

#if WITH_DEV_AUTOMATION_TESTS
public:
	/** 자동 테스트가 회복 직후 재충전이 즉시 실행되지 않고 Timer로 예약됐는지 확인합니다 */
	bool IsRechargeTimerActiveForTest() const;

	/** 자동 테스트가 재충전까지 남은 시간이 설정한 값인지 확인합니다 */
	float GetRechargeRemainingForTest() const;

	/** 자동 테스트가 전역 Frame을 조작하지 않고 재충전 완료 콜백을 실행합니다 */
	void CompleteRechargeForTest();

	/** 자동 테스트가 충전 직후 회복이 즉시 실행되지 않고 Timer로 예약됐는지 확인합니다 */
	bool IsChargedHealDelayActiveForTest() const;

	/** 자동 테스트가 회복까지 남은 시간이 설정한 값인지 확인합니다 */
	float GetChargedHealDelayRemainingForTest() const;

	/** 자동 테스트가 전역 Frame을 조작하지 않고 충전 후 회복 콜백을 실행합니다 */
	void CompleteChargedHealDelayForTest();
#endif
};
