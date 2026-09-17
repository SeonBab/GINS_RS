#include "RSGameplayAbility_Interact.h"

#include "AbilitySystemComponent.h"
#include "GameFramework/Actor.h"
#include "RSGameplayAbility_InteractionScan.h"
#include "RSGameplayTags.h"
#include "RSInteractable.h"

URSGameplayAbility_Interact::URSGameplayAbility_Interact()
{
	ActivationPolicy = ERSAbilityActivationPolicy::OnInputTriggered;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(RSGameplayTags::Ability_Interaction_Interact);
	SetAssetTags(AssetTags);

	// 대시나 넉다운처럼 행동이 잠긴 동안에는 상호작용도 막습니다
	ActivationBlockedTags.AddTag(RSGameplayTags::State_Action_Locked);
}

void URSGameplayAbility_Interact::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	const URSGameplayAbility_InteractionScan* ScanAbility = URSGameplayAbility_InteractionScan::FindInstance(GetAbilitySystemComponentFromActorInfo());
	AActor* FocusedActor = ScanAbility ? ScanAbility->GetFocusedInteractable() : nullptr;
	AActor* InteractInstigator = GetAvatarActorFromActorInfo();

	// 실행하지 못해도 잠긴 상태를 남기지 않도록 어느 경로든 어빌리티를 끝냅니다
	const bool bInteracted = TryInteractWithFocus(FocusedActor, InteractInstigator);

	EndAbility(Handle, ActorInfo, ActivationInfo, true, !bInteracted);
}

bool URSGameplayAbility_Interact::TryInteractWithFocus(AActor* FocusedActor, AActor* InteractInstigator)
{
	if (!IsValid(FocusedActor) || !FocusedActor->Implements<URSInteractable>())
	{
		return false;
	}

	// 탐색 주기 사이에 대상 상태가 바뀔 수 있으므로 선택 시점의 판정을 믿지 않고 다시 확인합니다
	if (!IRSInteractable::Execute_CanInteract(FocusedActor, InteractInstigator))
	{
		return false;
	}

	const TSubclassOf<URSBaseGameplayAbility> InteractAbilityClass = IRSInteractable::Execute_GetInteractAbilityClass(FocusedActor);
	if (!InteractAbilityClass)
	{
		IRSInteractable::Execute_Interact(FocusedActor, InteractInstigator);

		return true;
	}

	// 대상이 지정한 어빌리티는 선택된 동안 탐색 어빌리티가 미리 부여해 두었습니다
	return TryActivateAbilityByClass(GetAbilitySystemComponentFromActorInfo(), InteractAbilityClass);
}
