// Fill out your copyright notice in the Description page of Project Settings.


#include "RSAbilitySet.h"

#include "RSAbilitySystemComponent.h"
#include "Abilities/RSBaseGameplayAbility.h"

void FRSAbilitySet_GrantedHandles::AddAbilitySpecHandle(const FGameplayAbilitySpecHandle& Handle)
{
	if (!Handle.IsValid())
	{
		return;
	}

	AbilitySpecHandles.Add(Handle);
}

void FRSAbilitySet_GrantedHandles::TakeFromAbilitySystem(URSAbilitySystemComponent* AbilitySystemComponent)
{
	// GiveAbility와 ClearAbility는 ASC의 권한을 가진 쪽에서만 수행합니다
	if (!AbilitySystemComponent || !AbilitySystemComponent->IsOwnerActorAuthoritative())
	{
		return;
	}

	for (const FGameplayAbilitySpecHandle& Handle : AbilitySpecHandles)
	{
		if (!Handle.IsValid())
		{
			continue;
		}

		// AbilitySet을 통해 부여했던 어빌리티 스펙을 ASC에서 제거합니다
		AbilitySystemComponent->ClearAbility(Handle);
	}

	AbilitySpecHandles.Reset();
}

void URSAbilitySet::GiveToAbilitySystem(URSAbilitySystemComponent* AbilitySystemComponent, FRSAbilitySet_GrantedHandles* OutGrantedHandles, UObject* SourceObject) const
{
	// 싱글플레이에서도 Standalone 인스턴스가 권한을 가지므로 동일한 경로를 사용합니다
	if (!AbilitySystemComponent || !AbilitySystemComponent->IsOwnerActorAuthoritative())
	{
		return;
	}

	for (const FRSAbilitySet_GameplayAbility& AbilityToGrant : GrantedGameplayAbilities)
	{
		if (!AbilityToGrant.Ability)
		{
			UE_LOG(LogTemp, Error, TEXT("%s contains an invalid ability."), *GetNameSafe(this));

			continue;
		}

		FGameplayAbilitySpec AbilitySpec(AbilityToGrant.Ability, AbilityToGrant.AbilityLevel, INDEX_NONE, SourceObject);

		if (AbilityToGrant.InputTag.IsValid())
		{
			// 입력 처리 시 이 태그로 활성화할 어빌리티 스펙을 검색합니다
			AbilitySpec.GetDynamicSpecSourceTags().AddTag(AbilityToGrant.InputTag);
		}

		const FGameplayAbilitySpecHandle Handle = AbilitySystemComponent->GiveAbility(AbilitySpec);

		if (!OutGrantedHandles)
		{
			continue;
		}

		// AbilitySet이 해제될 때 이 어빌리티만 회수할 수 있도록 핸들을 기록합니다
		OutGrantedHandles->AddAbilitySpecHandle(Handle);
	}
}
