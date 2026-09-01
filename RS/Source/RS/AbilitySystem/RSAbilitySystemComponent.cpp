
// Fill out your copyright notice in the Description page of Project Settings.

#include "RSAbilitySystemComponent.h"

#include "Abilities/RSBaseGameplayAbility.h"
#include "RSGameplayTags.h"

void URSAbilitySystemComponent::AbilityInputTagPressed(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid() || HasMatchingGameplayTag(RSGameplayTags::State_Dead))
	{
		return;
	}

	for (const FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		if (!AbilitySpec.Ability)
		{
			continue;
		}

		if (!AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			continue;
		}

		// 누른 첫 프레임과 입력을 유지하는 동안의 상태를 각각 기록합니다
		InputPressedSpecHandles.AddUnique(AbilitySpec.Handle);
		InputHeldSpecHandles.AddUnique(AbilitySpec.Handle);
	}
}

void URSAbilitySystemComponent::AbilityInputTagReleased(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid())
	{
		return;
	}

	for (const FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		if (!AbilitySpec.Ability)
		{
			continue;
		}

		if (!AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			continue;
		}

		// 해제 상태를 한 프레임 동안 기록하고 유지 상태에서는 제거합니다
		InputReleasedSpecHandles.AddUnique(AbilitySpec.Handle);
		InputHeldSpecHandles.Remove(AbilitySpec.Handle);
	}
}

void URSAbilitySystemComponent::ProcessAbilityInput(float DeltaTime, bool bGamePaused)
{
	if (HasMatchingGameplayTag(RSGameplayTags::State_Dead))
	{
		ClearAbilityInput();
		return;
	}

	TArray<FGameplayAbilitySpecHandle> AbilitiesToActivate;

	// 입력을 유지하는 동안 비활성 상태인 WhileInputActive 어빌리티를 수집합니다
	for (const FGameplayAbilitySpecHandle& SpecHandle : InputHeldSpecHandles)
	{
		const FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromHandle(SpecHandle);

		if (!AbilitySpec || !AbilitySpec->Ability || AbilitySpec->IsActive())
		{
			continue;
		}

		const URSBaseGameplayAbility* RSAbility = Cast<URSBaseGameplayAbility>(AbilitySpec->Ability);

		if (!RSAbility)
		{
			continue;
		}

		if (RSAbility->GetActivationPolicy() == ERSAbilityActivationPolicy::WhileInputActive)
		{
			AbilitiesToActivate.AddUnique(AbilitySpec->Handle);
		}
	}

	// 이번 프레임에 입력이 시작된 OnInputTriggered 어빌리티를 수집합니다
	for (const FGameplayAbilitySpecHandle& SpecHandle : InputPressedSpecHandles)
	{
		FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromHandle(SpecHandle);

		if (!AbilitySpec || !AbilitySpec->Ability)
		{
			continue;
		}

		// 활성화 시 입력 상태를 참조할 수 있도록 Spec에 기록합니다
		AbilitySpec->InputPressed = true;

		if (AbilitySpec->IsActive())
		{
			// 이미 실행 중인 어빌리티에는 새로운 입력 이벤트를 전달합니다
			AbilitySpecInputPressed(*AbilitySpec);
			continue;
		}

		const URSBaseGameplayAbility* RSAbility = Cast<URSBaseGameplayAbility>(AbilitySpec->Ability);

		if (!RSAbility)
		{
			continue;
		}

		if (RSAbility->GetActivationPolicy() == ERSAbilityActivationPolicy::OnInputTriggered)
		{
			AbilitiesToActivate.AddUnique(AbilitySpec->Handle);
		}
	}

	// Spec 순회 중 상태가 변경되지 않도록 수집을 마친 뒤 활성화를 시도합니다
	for (const FGameplayAbilitySpecHandle& SpecHandle : AbilitiesToActivate)
	{
		TryActivateAbility(SpecHandle);
	}

	// 이번 프레임에 해제된 입력을 실행 중인 어빌리티에 전달합니다
	for (const FGameplayAbilitySpecHandle& SpecHandle : InputReleasedSpecHandles)
	{
		FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromHandle(SpecHandle);

		if (!AbilitySpec || !AbilitySpec->Ability)
		{
			continue;
		}

		AbilitySpec->InputPressed = false;

		if (AbilitySpec->IsActive())
		{
			AbilitySpecInputReleased(*AbilitySpec);
		}
	}

	// Pressed와 Released는 한 프레임 상태이므로 처리 후 초기화합니다
	// Held는 입력을 해제할 때까지 다음 프레임에도 유지합니다
	InputPressedSpecHandles.Reset();
	InputReleasedSpecHandles.Reset();
}

void URSAbilitySystemComponent::ClearAbilityInput()
{
	InputPressedSpecHandles.Reset();
	InputReleasedSpecHandles.Reset();
	InputHeldSpecHandles.Reset();
}

void URSAbilitySystemComponent::AbilitySpecInputPressed(FGameplayAbilitySpec& Spec)
{
	Super::AbilitySpecInputPressed(Spec);

	if (!Spec.IsActive())
	{
		return;
	}

	const UGameplayAbility* AbilityInstance = Spec.GetPrimaryInstance();
	if (!AbilityInstance)
	{
		return;
	}

	InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputPressed, Spec.Handle, AbilityInstance->GetCurrentActivationInfo().GetActivationPredictionKey());
}

void URSAbilitySystemComponent::AbilitySpecInputReleased(FGameplayAbilitySpec& Spec)
{
	Super::AbilitySpecInputReleased(Spec);

	if (!Spec.IsActive())
	{
		return;
	}

	const UGameplayAbility* AbilityInstance = Spec.GetPrimaryInstance();
	if (!AbilityInstance)
	{
		return;
	}

	InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputReleased, Spec.Handle, AbilityInstance->GetCurrentActivationInfo().GetActivationPredictionKey());
}
