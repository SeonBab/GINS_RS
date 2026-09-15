// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "RSBaseCharacter.generated.h"

class UAnimMontage;

UCLASS(Abstract)
class RS_API ARSBaseCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ARSBaseCharacter();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

protected:
	/** 사망 상태에 진입한 캐릭터가 DeathMontage를 한 번 재생합니다 */
	void PlayDeathMontage();

protected:
	/** 사망 상태가 시작될 때 재생할 Animation Montage입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Death")
	TObjectPtr<UAnimMontage> DeathMontage;
};
