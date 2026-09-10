// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "RSAbilityTask_ObserveAttackWindow.generated.h"

class UAnimMontage;

/** 관찰 중인 Montage Position이 갱신되면 발생합니다 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRSAttackWindowPositionUpdated, float, MontagePosition);

/** Attack Window에 진입하면 발생합니다 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRSAttackWindowBegan);

/** 직전 Alpha부터 현재 Alpha까지 판정해야 할 구간을 전달합니다 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRSAttackWindowAdvanced, float, PreviousAlpha, float, CurrentAlpha);

/** Attack Window의 끝까지 정상 진행하면 발생합니다 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRSAttackWindowEnded);

/** Montage 또는 Attack Window 계약이 유효하지 않거나 재생이 중단되면 발생합니다 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRSAttackWindowInvalidated);

/** Montage Position으로 하나의 Attack Window 진입과 진행 구간을 관찰합니다 */
UCLASS()
class RS_API URSAbilityTask_ObserveAttackWindow : public UAbilityTask
{
	GENERATED_BODY()

public:
	URSAbilityTask_ObserveAttackWindow(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Window 진입 전을 포함해 관찰 중인 Montage Position이 갱신될 때 발생합니다 */
	UPROPERTY(BlueprintAssignable, Category = "RS|Combat|Attack Window")
	FRSAttackWindowPositionUpdated OnPositionUpdated;

	/** Attack Window에 진입할 때 한 번 발생합니다 */
	UPROPERTY(BlueprintAssignable, Category = "RS|Combat|Attack Window")
	FRSAttackWindowBegan OnWindowBegan;

	/** 이번 Tick에 판정해야 할 진행률 구간이 있을 때 발생합니다 */
	UPROPERTY(BlueprintAssignable, Category = "RS|Combat|Attack Window")
	FRSAttackWindowAdvanced OnWindowAdvanced;

	/** Attack Window 끝까지 정상 재생했을 때 한 번 발생합니다 */
	UPROPERTY(BlueprintAssignable, Category = "RS|Combat|Attack Window")
	FRSAttackWindowEnded OnWindowEnded;

	/** 필수 구성 누락 또는 Montage 중단으로 Window를 완료할 수 없을 때 발생합니다 */
	UPROPERTY(BlueprintAssignable, Category = "RS|Combat|Attack Window")
	FRSAttackWindowInvalidated OnInvalidated;

	/** 재생 중인 필수 Montage의 유일한 Attack Window를 관찰합니다 */
	static URSAbilityTask_ObserveAttackWindow* ObserveAttackWindow(UGameplayAbility* OwningAbility, UAnimMontage* Montage);

	/** Montage에서 정확히 하나인 Attack Window의 시작과 끝을 반환합니다 */
	static bool TryGetAttackWindowRange(const UAnimMontage* Montage, float& OutWindowStartPosition, float& OutWindowEndPosition, int32* OutAttackWindowCount = nullptr);

	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;

private:
	/** 현재 Montage Position까지의 Window 상태 변화를 전달합니다 */
	bool AdvanceWindow(float CurrentMontagePosition);

	/** 실패 이벤트를 한 번 전달하고 Task를 종료합니다 */
	void InvalidateTask();

private:
	/** 관찰할 필수 Montage입니다 */
	TObjectPtr<UAnimMontage> MontageToObserve;

	float WindowStartPosition = 0.0f;
	float WindowEndPosition = 0.0f;
	float PreviousMontagePosition = 0.0f;
	bool bWindowActive = false;
};
