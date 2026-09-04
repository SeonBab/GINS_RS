// Fill out your copyright notice in the Description page of Project Settings.


#include "RSCheatManager.h"

#include "RSAbilitySystemComponent.h"
#include "RSGameplayTags.h"
#include "RSPlayerState.h"

void URSCheatManager::RS_ToggleDownedTag()
{
	const APlayerController* PlayerController = GetPlayerController();
	const ARSPlayerState* RSPlayerState = PlayerController ? PlayerController->GetPlayerState<ARSPlayerState>() : nullptr;
	URSAbilitySystemComponent* AbilitySystemComp = RSPlayerState ? RSPlayerState->GetRSAbilitySystemComponent() : nullptr;
	if (!AbilitySystemComp)
	{
		return;
	}

	// 어빌리티가 부여한 태그를 실수로 제거하지 않도록 이 치트가 직접 부여한 경우에만 제거합니다
	if (bDownedTagApplied)
	{
		AbilitySystemComp->RemoveLooseGameplayTag(RSGameplayTags::State_CrowdControl_Downed);
	}
	else
	{
		AbilitySystemComp->AddLooseGameplayTag(RSGameplayTags::State_CrowdControl_Downed);
	}

	bDownedTagApplied = !bDownedTagApplied;
}
