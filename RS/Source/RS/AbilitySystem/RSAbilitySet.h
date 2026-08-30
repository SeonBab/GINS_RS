// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "RSAbilitySet.generated.h"


class URSAbilitySystemComponent;
class URSBaseGameplayAbility;

/** AbilitySet을 통해 ASC에 부여할 단일 Gameplay Ability의 설정입니다 */
USTRUCT(BlueprintType)
struct FRSAbilitySet_GameplayAbility
{
	GENERATED_BODY()

public:
	/** 부여할 Gameplay Ability 클래스입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<URSBaseGameplayAbility> Ability = nullptr;

	/** ASC에 부여할 어빌리티 레벨입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "1"))
	int32 AbilityLevel = 1;

	/** 입력과 어빌리티 스펙을 연결하는 태그입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "InputTag.Ability"))
	FGameplayTag InputTag;
};

/**
 * AbilitySet이 ASC에 부여한 어빌리티의 핸들을 보관합니다
 * 캐릭터 교체나 AbilitySet 해제 시 해당 어빌리티만 안전하게 회수하는 데 사용합니다
 */
USTRUCT()
struct FRSAbilitySet_GrantedHandles
{
	GENERATED_BODY()

public:
	/** AbilitySet을 통해 부여된 유효한 어빌리티 핸들을 기록합니다 */
	void AddAbilitySpecHandle(const FGameplayAbilitySpecHandle& Handle);

	/** 기록된 어빌리티를 ASC에서 제거하고 보관 중인 핸들을 초기화합니다 */
	void TakeFromAbilitySystem(URSAbilitySystemComponent* AbilitySystemComponent);

private:
	UPROPERTY()
	TArray<FGameplayAbilitySpecHandle> AbilitySpecHandles;
};

/** 여러 Gameplay Ability의 부여 설정을 하나의 데이터 에셋으로 관리합니다 */
UCLASS(BlueprintType, Const)
class RS_API URSAbilitySet : public UDataAsset
{
	GENERATED_BODY()

public:
	/**
	 * 설정된 어빌리티를 ASC에 부여합니다
	 * OutGrantedHandles를 전달하면 이후 회수를 위해 부여된 핸들을 함께 기록합니다
	 */
	void GiveToAbilitySystem(URSAbilitySystemComponent* AbilitySystemComponent, FRSAbilitySet_GrantedHandles* OutGrantedHandles = nullptr, UObject* SourceObject = nullptr) const;

private:
	/** 이 AbilitySet이 ASC에 부여할 Gameplay Ability 목록입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Abilities", meta = (AllowPrivateAccess = "true"))
	TArray<FRSAbilitySet_GameplayAbility> GrantedGameplayAbilities;
};
