// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "RSAbilitySystemComponent.generated.h"

/** RS의 어빌리티 부여와 활성화, 입력 처리를 담당하는 ASC입니다 */
UCLASS()
class RS_API URSAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	/** 입력 태그와 연결된 어빌리티를 누름 상태로 기록합니다 */
	void AbilityInputTagPressed(const FGameplayTag& InputTag);

	/** 입력 태그와 연결된 어빌리티를 해제 상태로 기록합니다 */
	void AbilityInputTagReleased(const FGameplayTag& InputTag);

	/** 기록된 입력 상태를 활성화 정책에 따라 처리합니다 */
	void ProcessAbilityInput(float DeltaTime, bool bGamePaused);

	/** 현재 기록된 모든 어빌리티 입력 상태를 초기화합니다 */
	void ClearAbilityInput();

protected:
	/** 실행 중인 어빌리티에 입력 누름 이벤트를 전달합니다 */
	virtual void AbilitySpecInputPressed(FGameplayAbilitySpec& Spec) override;

	/** 실행 중인 어빌리티에 입력 해제 이벤트를 전달합니다 */
	virtual void AbilitySpecInputReleased(FGameplayAbilitySpec& Spec) override;

private:
	/**
	 * 입력 상태에 따라 어빌리티 스펙 핸들을 분리하여 관리합니다
	 * Pressed와 Released는 해당 프레임의 ProcessAbilityInput에서 처리한 뒤 초기화하고,
	 * Held는 입력을 해제하거나 ClearAbilityInput을 호출할 때까지 유지합니다
	 */

	/** 이번 프레임에 입력이 시작되어 한 번만 처리할 어빌리티입니다 */
	TArray<FGameplayAbilitySpecHandle> InputPressedSpecHandles;

	/** 이번 프레임에 입력이 해제되어 한 번만 처리할 어빌리티입니다 */
	TArray<FGameplayAbilitySpecHandle> InputReleasedSpecHandles;

	/** 입력을 해제할 때까지 유지 상태로 처리할 어빌리티입니다 */
	TArray<FGameplayAbilitySpecHandle> InputHeldSpecHandles;
};
