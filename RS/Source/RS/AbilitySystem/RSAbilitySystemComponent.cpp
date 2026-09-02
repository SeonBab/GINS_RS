
// Fill out your copyright notice in the Description page of Project Settings.

#include "RSAbilitySystemComponent.h"

#include "Abilities/RSBaseGameplayAbility.h"
#include "Effects/RSGameplayEffect_AnimationState.h"
#include "RSGameplayTags.h"

void URSAbilitySystemComponent::BeginAnimationGameplayState(UObject* Source, int32 MontageInstanceIdentifier, int32 NotifyInstanceIdentifier, const FGameplayTagContainer& StateTags)
{
	if (!Source || StateTags.IsEmpty())
	{
		return;
	}

	const FGameplayTag StateRootTag = FGameplayTag::RequestGameplayTag(TEXT("State"), false);
	for (const FGameplayTag& StateTag : StateTags)
	{
		if (!StateRootTag.IsValid() || !StateTag.MatchesTag(StateRootTag))
		{
			return;
		}
	}

	const FRSAnimationGameplayStateKey StateKey{ Source, MontageInstanceIdentifier, NotifyInstanceIdentifier };
	if (AnimationGameplayStateEffectHandles.Contains(StateKey))
	{
		return;
	}

	FGameplayEffectContextHandle EffectContext = MakeEffectContext();
	EffectContext.AddSourceObject(Source);

	FGameplayEffectSpecHandle EffectSpecHandle = MakeOutgoingSpec(URSGameplayEffect_AnimationStateLease::StaticClass(), 1.0f, EffectContext);
	if (!EffectSpecHandle.IsValid())
	{
		return;
	}

	// 공용 GE 정의는 상태를 고정하지 않고 각 Notify State 실행이 지정한 태그와 수명을 함께 소유합니다
	EffectSpecHandle.Data->DynamicGrantedTags.AppendTags(StateTags);

	const FActiveGameplayEffectHandle EffectHandle = ApplyGameplayEffectSpecToSelf(*EffectSpecHandle.Data.Get());
	if (EffectHandle.IsValid())
	{
		AnimationGameplayStateEffectHandles.Add(StateKey, EffectHandle);
	}
}

void URSAbilitySystemComponent::EndAnimationGameplayState(UObject* Source, int32 MontageInstanceIdentifier, int32 NotifyInstanceIdentifier)
{
	if (!Source)
	{
		return;
	}

	FActiveGameplayEffectHandle EffectHandle;
	const FRSAnimationGameplayStateKey StateKey{ Source, MontageInstanceIdentifier, NotifyInstanceIdentifier };
	if (!AnimationGameplayStateEffectHandles.RemoveAndCopyValue(StateKey, EffectHandle))
	{
		return;
	}

	if (EffectHandle.IsValid())
	{
		RemoveActiveGameplayEffect(EffectHandle);
	}
}

void URSAbilitySystemComponent::EndAnimationGameplayStates(UObject* Source)
{
	if (!Source)
	{
		return;
	}

	for (auto EffectHandleIterator = AnimationGameplayStateEffectHandles.CreateIterator(); EffectHandleIterator; ++EffectHandleIterator)
	{
		if (EffectHandleIterator.Key().Source.Get() != Source)
		{
			continue;
		}

		const FActiveGameplayEffectHandle EffectHandle = EffectHandleIterator.Value();
		EffectHandleIterator.RemoveCurrent();

		if (EffectHandle.IsValid())
		{
			RemoveActiveGameplayEffect(EffectHandle);
		}
	}
}

void URSAbilitySystemComponent::AbilityInputTagPressed(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid() || HasMatchingGameplayTag(RSGameplayTags::State_Dead))
	{
		return;
	}

	bool bHasInputTriggeredAbility = false;

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

		// 어떤 어빌리티 입력이든 보관해 둔 입력보다 최신 의도이므로 정책을 따지지 않고 기록합니다
		bAbilityInputPressedThisFrame = true;

		// OnInputTriggered는 누른 프레임에만, WhileInputActive는 유지하는 동안 활성화되므로 두 상태를 나눠 기록합니다
		InputPressedSpecHandles.AddUnique(AbilitySpec.Handle);
		InputHeldSpecHandles.AddUnique(AbilitySpec.Handle);

		const URSBaseGameplayAbility* RSAbility = Cast<URSBaseGameplayAbility>(AbilitySpec.Ability);

		if (RSAbility && RSAbility->GetActivationPolicy() == ERSAbilityActivationPolicy::OnInputTriggered)
		{
			bHasInputTriggeredAbility = true;
		}
	}

	// 매 프레임 갱신되는 Held와 달리 단발 입력은 이번 프레임에 활성화하지 못하면 유실되므로 재시도할 태그를 남깁니다
	// 같은 프레임에 여러 단발 입력이 들어오면 가장 마지막 입력만 유지합니다
	if (bHasInputTriggeredAbility)
	{
		InputTriggeredTag = InputTag;
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

		// Held에서 제거해 버튼을 놓은 뒤에는 WhileInputActive 어빌리티가 다시 활성화되지 않게 합니다
		// 진행 중인 실행은 중단하지 않으므로 현재 어빌리티는 정상적으로 끝납니다
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

	// Held는 버튼을 놓을 때까지 남아 매 프레임 다시 시도되므로, WhileInputActive 어빌리티에는 입력 보관이 필요하지 않습니다
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

	// 이 순회는 Spec 단위 상태 기록과 이벤트 전달만 담당합니다
	// 단발 어빌리티의 활성화는 태그로 Spec을 다시 찾아야 하므로 아래에서 따로 처리합니다
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
			// 콤보처럼 실행 중에 다음 입력을 받아야 하는 어빌리티가 이 이벤트로 처리합니다
			AbilitySpecInputPressed(*AbilitySpec);
		}
	}

	// Spec 순회 중 상태가 변경되지 않도록 수집을 마친 뒤 활성화를 시도합니다
	for (const FGameplayAbilitySpecHandle& SpecHandle : AbilitiesToActivate)
	{
		TryActivateAbility(SpecHandle);
	}

	// 새 어빌리티 입력이 들어왔다면 활성화 정책과 무관하게 기다리던 입력의 의도는 지나간 것으로 봅니다
	if (bAbilityInputPressedThisFrame)
	{
		ClearBufferedAbilityInput();
	}

	// 폐기는 위에서 끝났으므로 여기서는 이번 프레임 입력의 활성화와 보관만 판단합니다
	if (InputTriggeredTag.IsValid())
	{
		if (TryActivateInputTriggeredAbilities(InputTriggeredTag) == ERSInputActivationResult::FailedToActivate)
		{
			BufferedInputTag = InputTriggeredTag;
			BufferedInputRemainingTime = InputBufferDuration;
		}
	}
	else if (!bGamePaused)
	{
		// 일시 정지 중에는 남은 시간을 줄이지 않아 메뉴를 닫은 뒤에도 직전 입력이 그대로 유효합니다
		ProcessBufferedAbilityInput(DeltaTime);
	}

	// 해제는 실행 중인 어빌리티만 소비하므로 활성화를 시도하지 않습니다
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
	// Held는 입력을 해제할 때까지 유지하고, 활성화하지 못한 단발 입력은 버퍼가 이어서 관리합니다
	InputPressedSpecHandles.Reset();
	InputReleasedSpecHandles.Reset();
	InputTriggeredTag = FGameplayTag();
	bAbilityInputPressedThisFrame = false;
}

void URSAbilitySystemComponent::ClearAbilityInput()
{
	InputPressedSpecHandles.Reset();
	InputReleasedSpecHandles.Reset();
	InputHeldSpecHandles.Reset();
	InputTriggeredTag = FGameplayTag();
	bAbilityInputPressedThisFrame = false;

	ClearBufferedAbilityInput();
}

ERSInputActivationResult URSAbilitySystemComponent::TryActivateInputTriggeredAbilities(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid())
	{
		return ERSInputActivationResult::NoActivationCandidate;
	}

	TArray<FGameplayAbilitySpecHandle> AbilitiesToActivate;

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

		const URSBaseGameplayAbility* RSAbility = Cast<URSBaseGameplayAbility>(AbilitySpec.Ability);

		if (!RSAbility || RSAbility->GetActivationPolicy() != ERSAbilityActivationPolicy::OnInputTriggered)
		{
			continue;
		}

		// 실행 중인 어빌리티에는 앞선 Pressed 처리에서 입력 이벤트를 전달했으므로 활성화 대상에서만 제외하고,
		// 같은 입력 태그를 공유하는 다른 비활성 어빌리티는 계속 검사합니다
		if (AbilitySpec.IsActive())
		{
			continue;
		}

		AbilitiesToActivate.AddUnique(AbilitySpec.Handle);
	}

	if (AbilitiesToActivate.IsEmpty())
	{
		return ERSInputActivationResult::NoActivationCandidate;
	}

	// Spec 순회 중 상태가 변경되지 않도록 수집을 마친 뒤 활성화를 시도합니다
	for (const FGameplayAbilitySpecHandle& SpecHandle : AbilitiesToActivate)
	{
		// 활성화가 다른 어빌리티의 조건 태그를 바꿀 수 있으므로, 한 번의 입력이 여러 어빌리티를 연쇄 활성화하지 않게 첫 성공에서 종료합니다
		if (TryActivateAbility(SpecHandle))
		{
			return ERSInputActivationResult::Activated;
		}
	}

	return ERSInputActivationResult::FailedToActivate;
}

void URSAbilitySystemComponent::ProcessBufferedAbilityInput(float DeltaTime)
{
	if (!BufferedInputTag.IsValid())
	{
		return;
	}

	// 재시도는 활성화만 수행하며, 입력 이벤트를 다시 보내면 한 번 누른 입력이 두 번 처리됩니다
	const ERSInputActivationResult ActivationResult = TryActivateInputTriggeredAbilities(BufferedInputTag);

	// 활성화할 후보가 없다면 어빌리티가 이미 실행 중이거나 제거된 상태이므로 더 기다리지 않습니다
	if (ActivationResult != ERSInputActivationResult::FailedToActivate)
	{
		ClearBufferedAbilityInput();

		return;
	}

	// 만료된 입력을 남겨두면 잠금이 풀린 뒤 의도하지 않은 시점에 실행됩니다
	BufferedInputRemainingTime -= DeltaTime;

	if (BufferedInputRemainingTime <= 0.0f)
	{
		ClearBufferedAbilityInput();
	}
}

void URSAbilitySystemComponent::ClearBufferedAbilityInput()
{
	BufferedInputTag = FGameplayTag();
	BufferedInputRemainingTime = 0.0f;
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

	// 입력을 어빌리티에 직접 전달하지 않고 실행별 예측 키로 캐시해 두면, 입력 대기 Task가 아직 등록되지 않았어도
	// 등록하는 시점에 이 입력을 확인할 수 있고 같은 어빌리티의 이전 실행이 남긴 입력과 섞이지 않습니다
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
