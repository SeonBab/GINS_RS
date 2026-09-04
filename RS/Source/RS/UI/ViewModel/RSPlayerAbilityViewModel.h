// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameplayTagContainer.h"
#include "RSLocalPlayerViewModelBase.h"
#include "RSPlayerAbilityViewModel.generated.h"

class ARSPlayerCharacter;
class UEnhancedInputLocalPlayerSubsystem;
class UMaterialInterface;
class URSAbilitySlotViewModel;
class URSAbilitySystemComponent;
class URSInputConfig;

/** 슬롯 하나에 대한 ViewModel과 현재 구독 상태를 함께 보관합니다 */
USTRUCT()
struct FRSAbilitySlotBinding
{
	GENERATED_BODY()

	/** 위젯에 전달할 슬롯 ViewModel입니다 */
	UPROPERTY(Transient)
	TObjectPtr<URSAbilitySlotViewModel> ViewModel;

	/** 현재 이 슬롯을 대표하는 어빌리티입니다 */
	FGameplayAbilitySpecHandle ResolvedAbilityHandle;

	/** 현재 구독 중인 쿨다운 태그와 그 델리게이트 핸들이며 순서가 서로 대응합니다 */
	TArray<FGameplayTag> CooldownTags;
	TArray<FDelegateHandle> CooldownTagDelegateHandles;
};

/**
 * 플레이어의 어빌리티 슬롯 표시 값을 슬롯별 ViewModel로 나누어 제공합니다
 * 슬롯이 어떤 어빌리티를 대표하는지는 ASC의 Display Resolver가 정하며 이 ViewModel은 판정하지 않습니다
 * LocalPlayer 저장소가 ViewModel을 클래스당 하나만 보관하므로 슬롯별 ViewModel은 이 ViewModel이 소유합니다
 */
UCLASS(BlueprintType)
class RS_API URSPlayerAbilityViewModel : public URSLocalPlayerViewModelBase
{
	GENERATED_BODY()

public:
	/** 제거되기 전에 등록한 모든 구독을 해제합니다 */
	virtual void BeginDestroy() override;

	/** PlayerCharacter가 데이터 원본으로 등록되면 ASC의 어빌리티와 상태 변경을 연결합니다 */
	virtual void HandleSourceRegistered(UObject* Source) override;

	/** 현재 PlayerCharacter가 데이터 원본에서 해제되면 구독과 슬롯 값을 정리합니다 */
	virtual void HandleSourceUnregistered(UObject* Source) override;

public:
	/**
	 * 해당 입력 자리를 담당할 슬롯 ViewModel을 반환하며 없으면 생성합니다
	 * 슬롯 목록은 위젯이 정적으로 소유하므로 위젯이 자신의 입력 태그로 요청합니다
	 */
	UFUNCTION(BlueprintCallable, Category = "RS|Ability Slot")
	URSAbilitySlotViewModel* GetOrCreateSlotViewModel(FGameplayTag InputTag, UMaterialInterface* IconMaterial);

	/** 등록된 모든 슬롯의 표시 값을 현재 상태에 맞게 다시 계산합니다 */
	UFUNCTION(BlueprintCallable, Category = "RS|Ability Slot")
	void RefreshSlots();

private:
	/** 데이터 원본과 ASC를 연결하고 어빌리티, 표시 문맥 상태 변경을 구독합니다 */
	void BindToSource(ARSPlayerCharacter* InPlayerCharacter);

	/** 연결한 모든 구독을 해제하고 슬롯 표시 값을 초기 상태로 되돌립니다 */
	void UnbindFromSource();

	/** 한 슬롯이 대표하는 어빌리티를 다시 결정하고 표시 값과 쿨다운 구독을 갱신합니다 */
	void RefreshSlot(const FGameplayTag& InputTag, FRSAbilitySlotBinding& SlotBinding);

	/** 현재 대표 어빌리티의 쿨다운 상태를 슬롯에 반영합니다 */
	void UpdateSlotCooldown(const FRSAbilitySlotBinding& SlotBinding) const;

	/** 슬롯이 구독 중인 쿨다운 태그를 대표 어빌리티에 맞게 교체합니다 */
	void UpdateCooldownSubscription(FRSAbilitySlotBinding& SlotBinding, const FGameplayTagContainer& NewCooldownTags);

	/** 슬롯이 구독 중인 쿨다운 태그를 모두 해제합니다 */
	void ClearCooldownSubscription(FRSAbilitySlotBinding& SlotBinding);

	/** 현재 입력 매핑에서 해당 입력 자리에 연결된 키의 표시 문자열을 반환합니다 */
	FText GetInputKeyTextForInputTag(const FGameplayTag& InputTag) const;

	/**
	 * 키의 표시 문자열을 번역 전 원문으로 반환합니다
	 * 키 표기는 게임 언어와 무관하게 같아야 하므로 번역된 이름을 사용하지 않습니다
	 */
	static FText GetCultureInvariantKeyText(const FKey& Key);

	/** 표시할 아이콘을 미리 로드하여 슬롯이 교체되는 순간에 로드가 발생하지 않게 합니다 */
	UTexture2D* LoadAbilityIcon(const TSoftObjectPtr<UTexture2D>& IconPath);

	/** 부여된 어빌리티 목록이 바뀌면 모든 슬롯을 다시 결정합니다 */
	UFUNCTION()
	void HandleGrantedAbilitiesChanged(URSAbilitySystemComponent* InAbilitySystemComponent);

	/** 슬롯의 의미를 바꾸는 상태가 변하면 모든 슬롯을 다시 결정합니다 */
	void HandleDisplayContextTagChanged(const FGameplayTag ChangedTag, int32 NewCount);

	/** 쿨다운 상태가 변하면 해당 슬롯의 쿨다운 값만 갱신합니다 */
	void HandleCooldownTagChanged(const FGameplayTag ChangedTag, int32 NewCount);

	/**
	 * 입력 매핑이 다시 구성되면 슬롯의 키 표시를 갱신합니다
	 * Pawn을 소유하는 시점보다 MappingContext 추가가 늦으므로 최초 갱신도 이 경로를 거칩니다
	 */
	UFUNCTION()
	void HandleControlMappingsRebuilt();

private:
	/** 슬롯 값을 관찰하는 데이터 원본입니다 */
	UPROPERTY(Transient)
	TWeakObjectPtr<ARSPlayerCharacter> PlayerCharacter;

	/** 어빌리티와 상태 변경을 구독한 ASC입니다 */
	UPROPERTY(Transient)
	TWeakObjectPtr<URSAbilitySystemComponent> AbilitySystemComp;

	/** 입력 매핑 변경을 구독한 Enhanced Input 저장소이며 해제할 때 원본 없이도 찾을 수 있게 보관합니다 */
	UPROPERTY(Transient)
	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> EnhancedInputSubsystem;

	/** 입력 자리별 슬롯 ViewModel과 구독 상태입니다 */
	UPROPERTY(Transient)
	TMap<FGameplayTag, FRSAbilitySlotBinding> SlotBindings;

	/** 미리 로드한 아이콘이 사용 중에 회수되지 않도록 참조를 유지합니다 */
	UPROPERTY(Transient)
	TMap<FSoftObjectPath, TObjectPtr<UTexture2D>> LoadedIcons;

	/** 표시 문맥 상태 변경 구독을 정확히 해제하기 위한 태그와 핸들이며 순서가 서로 대응합니다 */
	TArray<FGameplayTag> DisplayContextTags;
	TArray<FDelegateHandle> DisplayContextTagDelegateHandles;
};
