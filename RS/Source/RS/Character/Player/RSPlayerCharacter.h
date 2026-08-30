// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RSBaseCharacter.h"
#include "AbilitySystemInterface.h"
#include "RSAbilitySet.h"
#include "RSPlayerCharacter.generated.h"

class UInputMappingContext;
class URSInputConfig;
class UCameraComponent;
class USpringArmComponent;
class URSHealthComponent;

struct FInputActionValue;

UCLASS()
class RS_API ARSPlayerCharacter : public ARSBaseCharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ARSPlayerCharacter();

	virtual void PossessedBy(AController* NewController) override;

	/** PlayerState가 복제된 후 ASC의 ActorInfo를 초기화합니다 */
	virtual void OnRep_PlayerState() override;

#pragma region Input
public:
	/** 플레이어 입력 컴포넌트에 입력 액션을 바인딩합니다 */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:
	void Input_Move(const FInputActionValue& InputActionValue);

	/** 어빌리티 입력 태그의 누름 상태를 ASC에 전달합니다 */
	void Input_AbilityTagPressed(FGameplayTag InputTag);

	/** 어빌리티 입력 태그의 해제 상태를 ASC에 전달합니다 */
	void Input_AbilityTagReleased(FGameplayTag InputTag);

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<URSInputConfig> InputConfig;
#pragma endregion

#pragma region GAS
public:
	/** PlayerState의 ASC를 현재 캐릭터 Avatar로 초기화하고 기본 AbilitySet을 한 번 부여합니다 */
	void InitializeAbilitySystem();

	/** PlayerState가 소유한 ASC를 반환합니다 */
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	/** PlayerState의 HealthSet을 현재 캐릭터에 연결하는 컴포넌트를 반환합니다 */
	UFUNCTION(BlueprintPure, Category = "RS|Health")
	URSHealthComponent* GetHealthComponent() const { return HealthComp; }

protected:
	/**
	 * 이 캐릭터가 초기화될 때 기본으로 부여할 AbilitySet입니다
	 * 현재는 캐릭터의 기본 어빌리티를 하나의 세트로 관리하고, 장비, 특성, 직업처럼 부여 출처가 늘어나면 배열이나 별도 컴포넌트로 확장합니다
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Abilities")
	TObjectPtr<URSAbilitySet> DefaultAbilitySet;

private:
	/** DefaultAbilitySet이 부여한 어빌리티를 나중에 정확히 회수하기 위한 핸들입니다 */
	UPROPERTY(Transient)
	FRSAbilitySet_GrantedHandles GrantedAbilityHandles;

	/** ASC 초기화가 여러 번 호출되어도 기본 어빌리티가 중복으로 부여되지 않게 합니다 */
	bool bDefaultAbilitiesGranted = false;
#pragma endregion

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Components|Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> CameraComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Components|Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> SpringArmComp;

private:
	/** PlayerState의 HealthSet 변경을 캐릭터와 UI에 전달합니다 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Health", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URSHealthComponent> HealthComp;
};
