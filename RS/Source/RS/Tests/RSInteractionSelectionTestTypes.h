// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#if WITH_TESTS

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RSInteractable.h"
#include "RSInteractionSelectionTestTypes.generated.h"

class USphereComponent;

/**
 * 선정 규칙만 검증하기 위한 자동화 테스트 전용 상호작용 대상입니다
 * 실제 대상은 Blueprint가 될 예정이므로 콘텐츠에 의존하지 않고 계약만 최소로 구현합니다
 * UHT는 WITH_DEV_AUTOMATION_TESTS를 해석하지 못해 안쪽 선언을 건너뛰므로 WITH_TESTS로 감쌉니다
 */
UCLASS()
class ARSInteractionSelectionTestTarget : public AActor, public IRSInteractable
{
	GENERATED_BODY()

public:
	ARSInteractionSelectionTestTarget();

	virtual bool CanInteract_Implementation(const AActor* InteractInstigator) const override { return bCanInteract; }

	/** 감지 형상이자 거리 측정 대상인 구체를 반환합니다 */
	USphereComponent* GetShape() const { return Shape; }

	/** 상호작용을 거부하는 대상이 후보에서 빠지는지 확인하기 위해 테스트가 값을 바꿉니다 */
	void SetCanInteract(bool bInCanInteract) { bCanInteract = bInCanInteract; }

private:
	UPROPERTY()
	TObjectPtr<USphereComponent> Shape;

	bool bCanInteract = true;
};

/** 인터페이스를 구현하지 않은 액터가 후보에서 빠지는지 확인하기 위한 대조 액터입니다 */
UCLASS()
class ARSInteractionSelectionTestNonTarget : public AActor
{
	GENERATED_BODY()

public:
	ARSInteractionSelectionTestNonTarget();

	USphereComponent* GetShape() const { return Shape; }

private:
	UPROPERTY()
	TObjectPtr<USphereComponent> Shape;
};

#endif // WITH_TESTS
