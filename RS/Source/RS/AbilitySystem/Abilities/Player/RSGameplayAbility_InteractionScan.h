#pragma once

#include "CoreMinimal.h"
#include "RSBaseGameplayAbility.h"
#include "RSGameplayAbility_InteractionScan.generated.h"

class UAbilitySystemComponent;
class URSInteractionPromptWidget;
class UWidgetComponent;

/**
 * Avatar 주변의 상호작용 대상을 상시 탐색하고 선택된 대상 위에 프롬프트를 표시합니다
 * 입력을 받지 않으며 실행은 URSGameplayAbility_Interact가 담당합니다
 * 선택된 대상을 이 인스턴스가 소유하므로 InstancingPolicy는 InstancedPerActor여야 합니다
 */
UCLASS()
class RS_API URSGameplayAbility_InteractionScan : public URSBaseGameplayAbility
{
	GENERATED_BODY()

public:
	URSGameplayAbility_InteractionScan();

	/**
	 * 해당 ASC에서 실행 중인 이 어빌리티의 인스턴스를 반환하며 없으면 nullptr을 반환합니다
	 * 선택 상태가 인스턴스에 있으므로 실행 어빌리티가 이 경로로 상태를 조회합니다
	 */
	static URSGameplayAbility_InteractionScan* FindInstance(const UAbilitySystemComponent* AbilitySystemComponent);

	/** 지금 선택된 상호작용 대상을 반환하며 없으면 nullptr을 반환합니다 */
	AActor* GetFocusedInteractable() const { return FocusedInteractable.Get(); }

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	/** 선택된 대상이 바뀌면 프롬프트와 대상 어빌리티 부여를 함께 갱신합니다 */
	UFUNCTION()
	void HandleFocusChanged(AActor* FocusedActor);

	/** Avatar에 프롬프트 위젯 컴포넌트를 하나 만들어 숨긴 상태로 둡니다 */
	void CreatePromptWidget();

	/** 프롬프트를 새 대상에 붙이고 문구를 갱신하며 대상이 없으면 숨깁니다 */
	void UpdatePromptForFocus(AActor* FocusedActor);

	/** 새 대상이 지정한 어빌리티를 부여하고 이전 대상의 부여는 회수합니다 */
	void UpdateGrantedAbilityForFocus(AActor* FocusedActor);

	/** 대상 어빌리티 부여를 회수합니다 */
	void ClearGrantedInteractAbility();

private:
	/** 대상을 찾을 반경이며 기준은 Avatar의 위치입니다 */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Interaction", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ScanRadius = 300.0f;

	/** 주변을 다시 훑는 간격입니다 */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Interaction", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ScanInterval = 0.1f;

	/** 상호작용 대상만 응답하는 Trace Channel입니다 */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Interaction")
	TEnumAsByte<ECollisionChannel> ScanChannel = ECollisionChannel::ECC_GameTraceChannel3;

	/** 대상 위에 표시할 프롬프트 Widget입니다 */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Interaction")
	TSubclassOf<URSInteractionPromptWidget> PromptWidgetClass;

	/** Avatar가 소유하는 프롬프트 위젯 컴포넌트이며 선택이 바뀔 때 대상으로 옮겨 붙습니다 */
	UPROPERTY(Transient)
	TObjectPtr<UWidgetComponent> PromptWidgetComp;

	/** 지금 선택된 상호작용 대상입니다 */
	TWeakObjectPtr<AActor> FocusedInteractable;

	/** 선택된 대상이 지정한 어빌리티의 부여 핸들이며 선택이 유지되는 동안에만 유효합니다 */
	FGameplayAbilitySpecHandle GrantedInteractAbilityHandle;
};
