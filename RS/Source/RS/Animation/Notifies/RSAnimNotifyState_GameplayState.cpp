// Fill out your copyright notice in the Description page of Project Settings.

#include "RSAnimNotifyState_GameplayState.h"

#include "AbilitySystemGlobals.h"
#include "Animation/ActiveMontageInstanceScope.h"
#include "Misc/DataValidation.h"
#include "RSAbilitySystemComponent.h"

namespace
{
	int32 GetMontageInstanceId(const FAnimNotifyEventReference& EventReference)
	{
		const UE::Anim::FAnimNotifyMontageInstanceContext* MontageContext = EventReference.GetContextData<UE::Anim::FAnimNotifyMontageInstanceContext>();

		return MontageContext ? MontageContext->MontageInstanceID : INDEX_NONE;
	}

	bool ContainsOnlyStateTags(const FGameplayTagContainer& GameplayTags)
	{
		const FGameplayTag StateRootTag = FGameplayTag::RequestGameplayTag(TEXT("State"), false);
		if (!StateRootTag.IsValid())
		{
			return false;
		}

		for (const FGameplayTag& GameplayTag : GameplayTags)
		{
			if (!GameplayTag.MatchesTag(StateRootTag))
			{
				return false;
			}
		}

		return true;
	}
}

URSAnimNotifyState_GameplayState::URSAnimNotifyState_GameplayState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 동시에 재생되는 같은 Montage 인스턴스가 하나의 Notify 수명으로 병합되지 않게 합니다
	NotifyStateBehaviorFlags = static_cast<uint8>(EAnimNotifyStateBehaviorFlags::NoMergeOnConcurrentPlay);
}

void URSAnimNotifyState_GameplayState::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!MeshComp || !Animation || StateTags.IsEmpty() || !ContainsOnlyStateTags(StateTags))
	{
		return;
	}

	URSAbilitySystemComponent* AbilitySystemComponent = Cast<URSAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(MeshComp->GetOwner()));
	if (!AbilitySystemComponent)
	{
		return;
	}

	AbilitySystemComponent->BeginAnimationGameplayState(Animation, GetMontageInstanceId(EventReference), EventReference.GetNotifyInstanceID(), StateTags);
}

void URSAnimNotifyState_GameplayState::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (!MeshComp || !Animation)
	{
		return;
	}

	URSAbilitySystemComponent* AbilitySystemComponent = Cast<URSAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(MeshComp->GetOwner()));
	if (!AbilitySystemComponent)
	{
		return;
	}

	AbilitySystemComponent->EndAnimationGameplayState(Animation, GetMontageInstanceId(EventReference), EventReference.GetNotifyInstanceID());
}

FString URSAnimNotifyState_GameplayState::GetNotifyName_Implementation() const
{
	return StateTags.IsEmpty() ? TEXT("Gameplay State (Invalid)") : FString::Printf(TEXT("Gameplay State: %s"), *StateTags.ToStringSimple());
}

#if WITH_EDITOR
EDataValidationResult URSAnimNotifyState_GameplayState::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult ValidationResult = Super::IsDataValid(Context);

	if (StateTags.IsEmpty())
	{
		Context.AddError(FText::FromString(TEXT("Gameplay State Notify에는 하나 이상의 State 태그가 필요합니다")));
		ValidationResult = EDataValidationResult::Invalid;
	}
	else if (!ContainsOnlyStateTags(StateTags))
	{
		Context.AddError(FText::FromString(TEXT("Gameplay State Notify에는 State 계층의 태그만 사용할 수 있습니다")));
		ValidationResult = EDataValidationResult::Invalid;
	}

	return ValidationResult;
}
#endif
