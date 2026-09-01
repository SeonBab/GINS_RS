// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RSBaseCharacter.h"
#include "AbilitySystemInterface.h"
#include "RSAbilitySet.h"
#include "RSPlayerCharacter.generated.h"

class UInputMappingContext;
class UAnimMontage;
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
	virtual void PostInitializeComponents() override;

	virtual void PossessedBy(AController* NewController) override;

	/** PlayerState가 복제된 후 ASC의 ActorInfo를 초기화합니다 */
	virtual void OnRep_PlayerState() override;

#pragma region Input
public:
	/** 플레이어 입력 컴포넌트에 입력 액션을 바인딩합니다 */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:
	/** PlayerController가 반환한 마우스 위치까지 Navigation 이동을 요청합니다 */
	void Input_MoveTo(const FInputActionValue& InputActionValue);

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

	/** 현재 플레이어 캐릭터가 사망 상태인지 반환합니다 */
	UFUNCTION(BlueprintPure, Category = "RS|Death")
	bool IsDead() const;

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

#pragma region Death

protected:
	/** 사망 애니메이션 또는 Ragdoll 같은 시각적 연출을 Blueprint에서 시작합니다 */
	UFUNCTION(BlueprintImplementableEvent, Category = "RS|Death", meta = (DisplayName = "Death Started"))
	void ReceiveDeathStarted();

private:
	/** 사망 시 어빌리티와 이동을 중지하고 Blueprint 사망 연출을 시작합니다 */
	UFUNCTION()
	void HandleDeathStarted(URSHealthComponent* InHealthComponent);

protected:
	/** 사망 상태가 시작될 때 재생할 Animation Montage입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Death")
	TObjectPtr<UAnimMontage> DeathMontage;

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
