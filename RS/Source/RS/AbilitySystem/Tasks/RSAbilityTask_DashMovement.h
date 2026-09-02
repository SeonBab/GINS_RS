// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "Curves/RichCurve.h"
#include "RSAbilityTask_DashMovement.generated.h"

class UCharacterMovementComponent;

/** 누적 거리 진행률 커브를 매 프레임 평가하여 충돌을 유지하며 대시를 이동시킵니다 */
UCLASS()
class RS_API URSAbilityTask_DashMovement : public UAbilityTask
{
	GENERATED_BODY()

public:
	URSAbilityTask_DashMovement(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 지정한 방향과 거리 진행률 커브로 이동하는 AbilityTask를 생성합니다 */
	static URSAbilityTask_DashMovement* CreateDashMovement(UGameplayAbility* OwningAbility, UCharacterMovementComponent* MovementComponent, const FVector& Direction, float Distance, float Duration, const FRichCurve& ProgressCurve);

	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;

private:
	/** 대시 이동을 적용할 CharacterMovementComponent입니다 */
	TWeakObjectPtr<UCharacterMovementComponent> CachedMovementComponent;

	/** 활성화 순간에 고정한 월드 이동 방향입니다 */
	FVector DashDirection = FVector::ZeroVector;

	/** 장애물에 막히지 않았을 때 소비할 전체 이동 거리입니다 */
	float DashDistance = 0.0f;

	/** 거리 진행률 커브를 끝까지 평가할 시간입니다 */
	float DashDuration = 0.0f;

	/** 현재 실행에서 경과한 시간입니다 */
	float ElapsedTime = 0.0f;

	/** 이전 프레임까지 커브가 소비한 목표 거리입니다 */
	float PreviousDesiredDistance = 0.0f;

	/** 정규화된 시간을 누적 이동 거리 비율로 변환합니다 */
	FRichCurve DistanceProgressCurve;
};
