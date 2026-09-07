// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "Curves/RichCurve.h"
#include "RSAbilityTask_CurveMovement.generated.h"

class UCharacterMovementComponent;
class USkeletalMeshComponent;

/**
 * 누적 거리 진행률 커브를 매 프레임 평가하여 충돌을 유지하며 캐릭터를 수평 이동시킵니다
 * 높이를 지정하면 캡슐은 지면에 둔 채 Mesh만 아치를 그려 떠오르는 것처럼 보이게 합니다
 */
UCLASS()
class RS_API URSAbilityTask_CurveMovement : public UAbilityTask
{
	GENERATED_BODY()

public:
	URSAbilityTask_CurveMovement(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 지정한 방향과 거리 진행률 커브로 이동하는 AbilityTask를 생성합니다 */
	static URSAbilityTask_CurveMovement* CreateCurveMovement(UGameplayAbility* OwningAbility, UCharacterMovementComponent* MovementComponent, const FVector& Direction, float Distance, float Duration, const FRichCurve& ProgressCurve);

	/**
	 * 이동하는 동안 Mesh를 아치 모양으로 띄웁니다
	 * 캡슐은 지면에 남으므로 이동, 충돌과 Navigation은 기존 로직 그대로 동작합니다
	 */
	void SetMeshArc(USkeletalMeshComponent* MeshComponent, float Height);

	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;
	virtual void OnDestroy(bool bInOwnerFinished) override;

private:
	/** 띄웠던 Mesh를 원래 상대 위치로 되돌립니다 */
	void RestoreMeshLocation();

	/** 이동을 적용할 CharacterMovementComponent입니다 */
	TWeakObjectPtr<UCharacterMovementComponent> CachedMovementComponent;

	/** 아치로 띄울 Mesh이며 지정하지 않으면 수평 이동만 합니다 */
	TWeakObjectPtr<USkeletalMeshComponent> CachedMeshComponent;

	/** 아치를 시작하기 전의 Mesh 상대 위치이며 종료 시 이 값으로 되돌립니다 */
	FVector MeshBaseRelativeLocation = FVector::ZeroVector;

	/** 아치의 정점 높이이며 0이면 띄우지 않습니다 */
	float ArcHeight = 0.0f;

	/** 활성화 순간에 고정한 월드 이동 방향입니다 */
	FVector MovementDirection = FVector::ZeroVector;

	/** 장애물에 막히지 않았을 때 소비할 전체 이동 거리입니다 */
	float MovementDistance = 0.0f;

	/** 거리 진행률 커브를 끝까지 평가할 시간입니다 */
	float MovementDuration = 0.0f;

	/** 현재 실행에서 경과한 시간입니다 */
	float ElapsedTime = 0.0f;

	/** 이전 프레임까지 커브가 소비한 목표 거리입니다 */
	float PreviousDesiredDistance = 0.0f;

	/** 정규화된 시간을 누적 이동 거리 비율로 변환합니다 */
	FRichCurve DistanceProgressCurve;
};
