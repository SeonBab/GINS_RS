// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "RSInteractable.generated.h"

class URSBaseGameplayAbility;
class USceneComponent;

UINTERFACE(MinimalAPI, Blueprintable)
class URSInteractable : public UInterface
{
	GENERATED_BODY()
};

/**
 * 플레이어가 다가가 상호작용할 수 있는 대상의 계약입니다
 * 감지 형상은 대상이 이미 가진 콜리전이 담당하므로 Interactable 채널에 응답하도록 설정해야 하며 이 인터페이스는 형상을 요구하지 않습니다
 * 탐색과 프롬프트 표시는 플레이어 쪽 Ability가 담당하므로 대상은 자신이 언제 선택되었는지 알지 못합니다
 */
class RS_API IRSInteractable
{
	GENERATED_BODY()

public:
	/**
	 * 지금 상호작용할 수 있는 상태인지 반환합니다
	 * 대상 선택과 실행 직전에 모두 이 함수를 지나므로 프롬프트가 떠 있는 것과 실행 가능 여부가 갈라지지 않습니다
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "RS|Interaction")
	bool CanInteract(const AActor* InteractInstigator) const;
	virtual bool CanInteract_Implementation(const AActor* InteractInstigator) const { return true; }

	/** 프롬프트에 표시할 문구를 반환합니다 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "RS|Interaction")
	FText GetPromptText() const;
	virtual FText GetPromptText_Implementation() const { return FText::GetEmpty(); }

	/**
	 * 프롬프트 위젯을 붙일 컴포넌트를 반환하며 nullptr이면 호출자가 RootComponent를 사용합니다
	 * 위치만 반환하면 움직이는 대상에서 위젯이 따라오지 못하므로 Attach 대상을 직접 받습니다
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "RS|Interaction")
	USceneComponent* GetPromptAnchor() const;
	virtual USceneComponent* GetPromptAnchor_Implementation() const { return nullptr; }

	/**
	 * 상호작용할 때 플레이어 ASC에서 활성화할 Ability를 반환하며 필요 없으면 nullptr을 반환합니다
	 * 플레이어가 Montage를 재생하거나 쿨다운이 필요한 상호작용에만 지정하고, 그 밖에는 Interact()만 사용합니다
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "RS|Interaction")
	TSubclassOf<URSBaseGameplayAbility> GetInteractAbilityClass() const;
	virtual TSubclassOf<URSBaseGameplayAbility> GetInteractAbilityClass_Implementation() const { return nullptr; }

	/**
	 * Ability를 지정하지 않은 대상의 상호작용 결과를 실행합니다
	 * GetInteractAbilityClass()가 Ability를 반환하면 호출되지 않습니다
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "RS|Interaction")
	void Interact(AActor* InteractInstigator);
	virtual void Interact_Implementation(AActor* InteractInstigator) {}
};
