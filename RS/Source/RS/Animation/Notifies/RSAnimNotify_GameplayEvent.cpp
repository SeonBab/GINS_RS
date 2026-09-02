// Fill out your copyright notice in the Description page of Project Settings.

#include "RSAnimNotify_GameplayEvent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemGlobals.h"
#include "Misc/DataValidation.h"

URSAnimNotify_GameplayEvent::URSAnimNotify_GameplayEvent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bShouldFireInEditor = false;
}

void URSAnimNotify_GameplayEvent::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp || !EventTag.IsValid())
	{
		return;
	}

	AActor* OwnerActor = MeshComp->GetOwner();

	// Persona 프리뷰처럼 ASC가 없는 환경에서는 이벤트를 받을 대상이 없으므로 아무 동작도 하지 않습니다
	if (!OwnerActor || !UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwnerActor))
	{
		return;
	}

	FGameplayEventData EventData;
	EventData.EventTag = EventTag;
	EventData.Instigator = OwnerActor;
	EventData.Target = OwnerActor;

	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwnerActor, EventTag, EventData);
}

FString URSAnimNotify_GameplayEvent::GetNotifyName_Implementation() const
{
	return EventTag.IsValid() ? FString::Printf(TEXT("Gameplay Event: %s"), *EventTag.ToString()) : TEXT("Gameplay Event (Invalid)");
}

#if WITH_EDITOR
EDataValidationResult URSAnimNotify_GameplayEvent::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult ValidationResult = Super::IsDataValid(Context);

	if (!EventTag.IsValid())
	{
		Context.AddError(FText::FromString(TEXT("Gameplay Event Notify에는 보낼 GameplayEvent 태그가 필요합니다")));
		ValidationResult = EDataValidationResult::Invalid;
	}

	return ValidationResult;
}
#endif
