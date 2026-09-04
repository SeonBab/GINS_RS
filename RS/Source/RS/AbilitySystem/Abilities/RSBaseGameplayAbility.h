// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "ScalableFloat.h"
#include "RSBaseGameplayAbility.generated.h"

class UAnimMontage;
class UGameplayEffect;

/**
 * RS 프레임워크가 어빌리티 활성화를 시도하는 시점을 정의합니다
 * [참고] https://dev.epicgames.com/documentation/unreal-engine/abilities-in-lyra-in-unreal-engine#%ED%99%9C%EC%84%B1%ED%99%94%EC%A0%95%EC%B1%85
 */
UENUM(BlueprintType)
enum class ERSAbilityActivationPolicy : uint8
{
	None				UMETA(DisplayName = "None",					ToolTip = "자동으로 활성화하지 않으며 게임 코드, 블루프린트 또는 Gameplay Event에서 직접 활성화합니다."),
	OnInputTriggered	UMETA(DisplayName = "On Input Triggered",	ToolTip = "입력이 시작될 때 어빌리티 활성화를 한 번 시도합니다."),
	WhileInputActive	UMETA(DisplayName = "While Input Active",	ToolTip = "입력이 유지되는 동안 비활성 상태인 어빌리티의 활성화를 계속 시도합니다."),
	OnSpawn				UMETA(DisplayName = "On Spawn",				ToolTip = "유효한 Avatar가 설정되면 입력 없이 어빌리티 활성화를 시도합니다.")
};

UCLASS(Abstract, Blueprintable)
class RS_API URSBaseGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	/** Dead 상태에서 활성화되지 않는 RS 어빌리티의 공통 기본값을 구성합니다 */
	URSBaseGameplayAbility();

	/** 이 어빌리티의 자동 활성화 정책을 반환합니다 */
	ERSAbilityActivationPolicy GetActivationPolicy() const;

protected:
	/**
	 * ASC에 부여된 어빌리티 하나를 클래스로 찾아 활성화합니다
	 * 상태 흐름의 다음 단계로 넘길 때 사용하며, 실패하면 호출자가 잠긴 상태를 남기지 않고 정리해야 합니다
	 * EndAbility 이후에도 호출할 수 있도록 어빌리티의 현재 ActorInfo가 아니라 ASC를 직접 받습니다
	 */
	static bool TryActivateAbilityByClass(UAbilitySystemComponent* AbilitySystemComponent, TSubclassOf<UGameplayAbility> AbilityClass);

	/**
	 * 판정에 걸린 대상 하나에게 공용 대미지 GameplayEffect를 적용합니다
	 * `MakeOutgoingGameplayEffectSpec`이 UGameplayAbility의 protected 멤버라 함수 라이브러리로는 옮길 수 없습니다
	 */
	void ApplyDamageToTarget(AActor* TargetActor, TSubclassOf<UGameplayEffect> DamageEffectClass, float DamageAmount);

	/**
	 * 이 어빌리티가 재생한 Montage가 남긴 공용 게임플레이 상태를 회수합니다
	 * Montage가 중단되면 Notify State의 End가 오지 않아 상태 태그가 남으므로 EndAbility에서 호출합니다
	 * 어빌리티의 수명은 정하지 않으므로 Montage로 수명을 결정하지 않는 어빌리티도 그대로 사용합니다
	 */
	static void EndAnimationGameplayStatesForMontage(const FGameplayAbilityActorInfo* ActorInfo, UAnimMontage* Montage);

	/** 이 어빌리티의 재활성화를 차단하는 쿨다운 Tag를 반환합니다 */
	virtual const FGameplayTagContainer* GetCooldownTags() const override;

	/** 공용 GameplayEffect에 실행별 쿨다운 시간과 Tag를 설정하여 적용합니다 */
	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;

protected:
	/** RS 프레임워크가 사용할 자동 활성화 정책입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Activation")
	ERSAbilityActivationPolicy ActivationPolicy = ERSAbilityActivationPolicy::None;

	/** 어빌리티 레벨에 따라 적용할 쿨다운 시간입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Cooldown")
	FScalableFloat CooldownDuration = 0.0f;

	/** 쿨다운 중 이 어빌리티의 재활성화를 차단하는 Tag입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Cooldown", meta = (Categories = "Cooldown"))
	FGameplayTagContainer CooldownTags;
};
