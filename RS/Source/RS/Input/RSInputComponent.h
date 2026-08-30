// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EnhancedInputComponent.h"
#include "GameplayTagContainer.h"
#include "RSInputConfig.h"
#include "RSInputComponent.generated.h"

/** InputConfig의 입력 액션을 Gameplay Tag 기반 콜백에 연결하는 입력 컴포넌트입니다 */
UCLASS()
class RS_API URSInputComponent : public UEnhancedInputComponent
{
	GENERATED_BODY()

public:
	/** 지정한 태그와 일치하는 Native Input Action을 하나의 콜백에 바인딩합니다 */
	template<class UserClass, typename FuncType>
	void BindNativeAction(const URSInputConfig* InputConfig, const FGameplayTag& InputTag, ETriggerEvent TriggerEvent, UserClass* Object, FuncType Func, bool bLogIfNotFound = true);

	/** Ability Input Action의 누름과 해제 이벤트를 입력 태그와 함께 각각의 콜백에 바인딩합니다 */
	template<class UserClass, typename PressedFuncType, typename ReleasedFuncType>
	void BindAbilityActions(const URSInputConfig* InputConfig, UserClass* Object, PressedFuncType PressedFunc, ReleasedFuncType ReleasedFunc);
};

template<class UserClass, typename FuncType>
void URSInputComponent::BindNativeAction(const URSInputConfig* InputConfig, const FGameplayTag& InputTag, ETriggerEvent TriggerEvent, UserClass* Object, FuncType Func, bool bLogIfNotFound)
{
	check(InputConfig);

	if (const UInputAction* InputAction = InputConfig->FindNativeInputAction(InputTag, bLogIfNotFound))
	{
		BindAction(InputAction, TriggerEvent, Object, Func);
	}
}

template<class UserClass, typename PressedFuncType, typename ReleasedFuncType>
void URSInputComponent::BindAbilityActions(const URSInputConfig* InputConfig, UserClass* Object, PressedFuncType PressedFunc, ReleasedFuncType ReleasedFunc)
{
	check(InputConfig);

	for (const FRSInputAction& Action : InputConfig->GetAbilityInputActions())
	{
		if (!Action.InputAction || !Action.InputTag.IsValid())
		{
			continue;
		}

		// 입력이 시작된 순간 태그와 함께 누름 콜백을 호출합니다
		BindAction(Action.InputAction, ETriggerEvent::Started, Object, PressedFunc, Action.InputTag);

		// 정상적으로 입력을 놓으면 태그와 함께 해제 콜백을 호출합니다
		BindAction(Action.InputAction, ETriggerEvent::Completed, Object, ReleasedFunc, Action.InputTag);

		// 입력 조건이 중간에 취소되어도 ASC의 Held 상태가 남지 않도록 해제 콜백을 호출합니다
		BindAction(Action.InputAction, ETriggerEvent::Canceled, Object, ReleasedFunc, Action.InputTag);
	}
}
