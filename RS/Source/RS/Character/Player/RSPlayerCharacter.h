// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RSBaseCharacter.h"
#include "AbilitySystemInterface.h"
#include "RSAbilitySet.h"
#include "RSPlayerCharacter.generated.h"

class UInputMappingContext;
class UAnimMontage;
class UAbilitySystemComponent;
class UNiagaraSystem;
class URSInputConfig;
class UCameraComponent;
class USkeletalMeshComponent;
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
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void PossessedBy(AController* NewController) override;

	/** PlayerState가 복제된 후 ASC의 ActorInfo를 초기화합니다 */
	virtual void OnRep_PlayerState() override;

protected:
	/** 스폰 방향을 카메라 방위로 고정합니다 */
	virtual void BeginPlay() override;

#pragma region Input
public:
	/** 플레이어 입력 컴포넌트에 입력 액션을 바인딩합니다 */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	/** 입력 태그와 InputAction의 연결을 정의한 설정을 반환합니다 */
	const URSInputConfig* GetInputConfig() const { return InputConfig; }

protected:
	/** 우클릭을 누른 순간 최초 Navigation 이동과 클릭 위치 표시를 요청합니다 */
	void Input_MoveToStarted();

	/** PlayerController가 반환한 마우스 위치까지 Navigation 이동을 요청합니다 */
	void Input_MoveTo(const FInputActionValue& InputActionValue);

	/** 어빌리티 입력 태그의 누름 상태를 ASC에 전달합니다 */
	void Input_AbilityTagPressed(FGameplayTag InputTag);

	/** 어빌리티 입력 태그의 해제 상태를 ASC에 전달합니다 */
	void Input_AbilityTagReleased(FGameplayTag InputTag);

private:
	/** PlayerController에서 마우스 커서 아래의 이동 요청 위치를 가져옵니다 */
	bool TryGetMoveToLocation(FVector& OutMoveToLocation) const;

	/** 요청 위치를 NavMesh에 투영하고, 실패하면 같은 방향으로 도달할 수 있는 마지막 위치를 반환합니다 */
	bool TryResolveNavigableLocation(const FVector& RequestedLocation, FVector& OutNavigableLocation) const;

	/** 이동 요청 위치에 MoveClick Niagara를 한 번 재생합니다 */
	void SpawnMoveClickEffect(const FVector& Location) const;

	/** 현재 Gameplay State에서 새 Navigation 이동을 요청할 수 있는지 반환합니다 */
	bool CanRequestMoveTo() const;

	/** 진행 중인 Navigation 경로 추종과 CharacterMovement의 속도를 중단합니다 */
	void StopNavigationMovement();

	/** 현재 ASC의 행동 및 이동 차단 상태 변경을 구독합니다 */
	void InitializeMovementBlocking(UAbilitySystemComponent* AbilitySystemComp);

	/** 이전 ASC에 등록한 이동 차단 상태 변경 구독을 해제합니다 */
	void UninitializeMovementBlocking();

	/** 행동 또는 이동 차단 상태가 시작되면 진행 중인 Navigation 이동을 중단합니다 */
	void HandleMovementBlockingTagChanged(FGameplayTag GameplayTag, int32 NewCount);

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<URSInputConfig> InputConfig;

	/** 우클릭을 누른 위치에 한 번 재생할 Niagara System입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Input|Presentation")
	TObjectPtr<UNiagaraSystem> MoveClickSystem;

	/** 우클릭을 누르는 동안 커서 위치와 Navigation 경로를 갱신할 최소 시간 간격입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Input", meta = (ClampMin = "0.0", Units = "s"))
	float MoveToUpdateInterval = 0.1f;

	/** 이동 요청 위치를 NavMesh로 보정할 때 사용할 검색 범위입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Input", meta = (Units = "cm"))
	FVector MoveToProjectionExtent = FVector(150.0, 150.0, 500.0);

	/** 보정에 실패한 클릭을 NavMesh 경계까지 대신 이동시킬 때 요구하는 최소 이동 거리입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Input", meta = (ClampMin = "0.0", Units = "cm"))
	float MoveToMinimumFallbackDistance = 50.0f;

private:
	/** Started에서 최초 이동을 요청한 엔진 프레임입니다 */
	uint64 LastInitialMoveToRequestFrame = MAX_uint64;

	/** 마지막으로 커서 위치 갱신을 시도한 월드 시간입니다 */
	double LastMoveToUpdateTime = -TNumericLimits<double>::Max();

	/** 이동 차단 상태 변경을 구독한 ASC입니다 */
	TWeakObjectPtr<UAbilitySystemComponent> MovementStateAbilitySystemComp;

	/** 행동 차단 상태 변경 델리게이트를 정확히 해제하기 위한 핸들입니다 */
	FDelegateHandle ActionLockedTagDelegateHandle;

	/** 이동 차단 상태 변경 델리게이트를 정확히 해제하기 위한 핸들입니다 */
	FDelegateHandle MovementBlockedTagDelegateHandle;
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
	/** 본체 Mesh의 Leader Pose를 공유하며 외곽선 Material만 렌더링하는 Mesh입니다 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Presentation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> OutlineMeshComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Components|Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> CameraComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Components|Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> SpringArmComp;

private:
	/** PlayerState의 HealthSet 변경을 캐릭터와 UI에 전달합니다 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Health", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URSHealthComponent> HealthComp;
};
