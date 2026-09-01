// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "RSBaseGameplayAbility.generated.h"

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
	/** RS 프레임워크가 사용할 자동 활성화 정책입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Activation")
	ERSAbilityActivationPolicy ActivationPolicy = ERSAbilityActivationPolicy::None;
};
