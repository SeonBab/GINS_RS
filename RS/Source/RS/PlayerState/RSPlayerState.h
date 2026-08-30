// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "RSPlayerState.generated.h"

class URSAbilitySystemComponent;
class URSHealthSet;

UCLASS()
class RS_API ARSPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ARSPlayerState();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	URSAbilitySystemComponent* GetRSAbilitySystemComponent() const;

	/** 이 PlayerState가 소유한 체력 AttributeSet을 반환합니다 */
	const URSHealthSet* GetHealthSet() const;

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<URSAbilitySystemComponent> AbilitySystemComp;

	/** ASC와 같은 생명주기로 유지되는 체력 AttributeSet입니다 */
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<URSHealthSet> HealthSet;
};
