// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "AbilitySystemInterface.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RSCombatShapeTestTypes.generated.h"

class UBoxComponent;
class URSAbilitySystemComponent;

/** 실제 Overlap과 ASC 대상 필터를 통과하는 전투 형상 자동화 테스트용 Actor입니다 */
UCLASS()
class ARSCombatShapeTestActor : public AActor, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ARSCombatShapeTestActor();

	/** 판정 대상 유효성 검사에 사용할 ASC를 반환합니다 */
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UBoxComponent> CollisionComp;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<URSAbilitySystemComponent> AbilitySystemComp;
};
