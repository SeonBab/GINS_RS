// Fill out your copyright notice in the Description page of Project Settings.

#include "RSBaseGameplayAbility.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Combat/RSCombatFunctionLibrary.h"
#include "GameplayEffect.h"
#include "RSAbilityDefinition.h"
#include "RSAbilitySystemComponent.h"
#include "RSGameplayTags.h"
#include "RSPlayerController.h"
#include "RSHealthSet.h"

URSBaseGameplayAbility::URSBaseGameplayAbility()
{
	ActivationBlockedTags.AddTag(RSGameplayTags::State_Dead);
}

ERSAbilityActivationPolicy URSBaseGameplayAbility::GetActivationPolicy() const
{
	return ActivationPolicy;
}

const URSAbilityDefinition* URSBaseGameplayAbility::GetAbilityDefinition() const
{
	return AbilityDefinition;
}

bool URSBaseGameplayAbility::SatisfiesDisplayContext(const FGameplayTagContainer& OwnedTags, const FGameplayTagContainer& DisplayContextTags) const
{
	// 계층 일치를 허용하면 부모 태그를 조건으로 쓴 어빌리티가 의도치 않게 걸리므로 정확히 일치하는 태그만 남깁니다
	const FGameplayTagContainer RequiredContextTags = ActivationRequiredTags.FilterExact(DisplayContextTags);
	if (!OwnedTags.HasAll(RequiredContextTags))
	{
		return false;
	}

	const FGameplayTagContainer BlockedContextTags = ActivationBlockedTags.FilterExact(DisplayContextTags);

	return !OwnedTags.HasAny(BlockedContextTags);
}

bool URSBaseGameplayAbility::TryActivateAbilityByClass(UAbilitySystemComponent* AbilitySystemComponent, TSubclassOf<UGameplayAbility> AbilityClass)
{
	if (!AbilitySystemComponent || !AbilityClass)
	{
		return false;
	}

	// 분류 Tag가 아니라 클래스로 찾아 한 번의 전이가 여러 어빌리티를 켜지 않게 합니다
	const FGameplayAbilitySpec* AbilitySpec = AbilitySystemComponent->FindAbilitySpecFromClass(AbilityClass);
	if (!AbilitySpec)
	{
		return false;
	}

	return AbilitySystemComponent->TryActivateAbility(AbilitySpec->Handle);
}

const FGameplayTagContainer* URSBaseGameplayAbility::GetCooldownTags() const
{
	// 공용 GameplayEffect에는 특정 Ability의 Tag를 고정하지 않고 Ability 설정을 활성화 차단 기준으로 사용합니다
	if (!CooldownTags.IsEmpty())
	{
		return &CooldownTags;
	}

	// 프로젝트 공통 쿨다운 설정이 없는 Ability는 Unreal의 기본 CooldownGameplayEffect 방식을 유지합니다
	return Super::GetCooldownTags();
}

void URSBaseGameplayAbility::ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	// 쿨다운을 사용하는 Ability는 활성화 직후 CommitAbility를 호출하며 이 함수가 실행되는 시점부터 쿨다운이 시작됩니다
	if (CooldownTags.IsEmpty())
	{
		Super::ApplyCooldown(Handle, ActorInfo, ActivationInfo);

		return;
	}

	UGameplayEffect* CooldownGameplayEffect = GetCooldownGameplayEffect();
	if (!ensureMsgf(CooldownGameplayEffect, TEXT("%s에 CooldownTags가 설정되었지만 CooldownGameplayEffectClass가 없습니다"), *GetName()))
	{
		return;
	}

	const float AbilityLevel = GetAbilityLevel(Handle, ActorInfo);
	const float Duration = CooldownDuration.GetValueAtLevel(AbilityLevel);
	if (!ensureMsgf(Duration > 0.0f, TEXT("%s의 CooldownDuration은 0보다 커야 합니다"), *GetName()))
	{
		return;
	}

	// 공유 GameplayEffect 정의를 수정하지 않고 실행별 Spec에 각 Ability의 시간과 Tag를 기록합니다
	FGameplayEffectSpecHandle CooldownSpecHandle = MakeOutgoingGameplayEffectSpec(Handle, ActorInfo, ActivationInfo, CooldownGameplayEffect->GetClass(), AbilityLevel);
	if (!ensureMsgf(CooldownSpecHandle.IsValid(), TEXT("%s의 쿨다운 GameplayEffectSpec을 생성하지 못했습니다"), *GetName()))
	{
		return;
	}

	// Active GameplayEffect가 Tag 수명을 소유하므로 만료 시 별도 Timer나 수동 제거 없이 함께 정리됩니다
	CooldownSpecHandle.Data->DynamicGrantedTags.AppendTags(CooldownTags);
	CooldownSpecHandle.Data->SetSetByCallerMagnitude(RSGameplayTags::SetByCaller_Cooldown_Duration, Duration);

	ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, CooldownSpecHandle);
}

void URSBaseGameplayAbility::ApplyDamageToTarget(AActor* TargetActor, TSubclassOf<UGameplayEffect> DamageEffectClass, float DamageAmount)
{
	if (!TargetActor || !DamageEffectClass || !CurrentActorInfo || !CurrentActorInfo->AbilitySystemComponent.IsValid())
	{
		return;
	}

	UAbilitySystemComponent* TargetAbilitySystemComp = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor);
	if (!TargetAbilitySystemComp)
	{
		return;
	}

	const float AbilityLevel = GetAbilityLevel(CurrentSpecHandle, CurrentActorInfo);
	FGameplayEffectSpecHandle DamageSpecHandle = MakeOutgoingGameplayEffectSpec(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, DamageEffectClass, AbilityLevel);
	if (!DamageSpecHandle.IsValid())
	{
		return;
	}

	// 공용 대미지 GameplayEffect에 피해량을 고정하지 않고 이번 타격의 값을 실행별로 설정합니다
	DamageSpecHandle.Data->SetSetByCallerMagnitude(RSGameplayTags::SetByCaller_Damage, DamageAmount);
	CurrentActorInfo->AbilitySystemComponent->ApplyGameplayEffectSpecToTarget(*DamageSpecHandle.Data.Get(), TargetAbilitySystemComp);

	if (URSCombatFunctionLibrary::IsHitCheckDebugEnabled())
	{
		const URSHealthSet* TargetHealthSet = TargetAbilitySystemComp->GetSet<URSHealthSet>();
		UE_LOG(LogTemp, Log, TEXT("%s applied %.1f damage to %s, remaining health %.1f"), *GetName(), DamageAmount, *GetNameSafe(TargetActor), TargetHealthSet ? TargetHealthSet->GetHealth() : -1.0f);
	}
}

void URSBaseGameplayAbility::EndAnimationGameplayStatesForMontage(const FGameplayAbilityActorInfo* ActorInfo, UAnimMontage* Montage)
{
	if (!Montage || !ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid())
	{
		return;
	}

	URSAbilitySystemComponent* AbilitySystemComponent = Cast<URSAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get());
	if (!AbilitySystemComponent)
	{
		return;
	}

	AbilitySystemComponent->EndAnimationGameplayStates(Montage);
}

FVector URSBaseGameplayAbility::GetCursorDirectionOrForward(const FGameplayAbilityActorInfo* ActorInfo, const AActor& Actor)
{
	const ARSPlayerController* PlayerController = ActorInfo ? Cast<ARSPlayerController>(ActorInfo->PlayerController.Get()) : nullptr;
	if (PlayerController)
	{
		FVector CursorWorldLocation;
		if (PlayerController->GetCursorWorldLocation(CursorWorldLocation))
		{
			FVector CursorDirection = CursorWorldLocation - Actor.GetActorLocation();
			CursorDirection.Z = 0.0f;

			if (CursorDirection.Normalize())
			{
				return CursorDirection;
			}
		}
	}

	FVector ForwardDirection = Actor.GetActorForwardVector();
	ForwardDirection.Z = 0.0f;
	ForwardDirection.Normalize();

	return ForwardDirection;
}
