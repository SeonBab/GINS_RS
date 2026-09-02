// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagContainer.h"
#include "RSAbilitySystemComponent.generated.h"

/** 동시에 실행되는 애니메이션과 Notify State를 구분하는 게임플레이 상태 Lease 키입니다 */
struct FRSAnimationGameplayStateKey
{
	TWeakObjectPtr<UObject> Source;
	int32 MontageInstanceIdentifier = INDEX_NONE;
	int32 NotifyInstanceIdentifier = INDEX_NONE;

	bool operator==(const FRSAnimationGameplayStateKey& Other) const
	{
		return Source == Other.Source && MontageInstanceIdentifier == Other.MontageInstanceIdentifier && NotifyInstanceIdentifier == Other.NotifyInstanceIdentifier;
	}

	friend uint32 GetTypeHash(const FRSAnimationGameplayStateKey& Key)
	{
		return HashCombineFast(HashCombineFast(GetTypeHash(Key.Source), GetTypeHash(Key.MontageInstanceIdentifier)), GetTypeHash(Key.NotifyInstanceIdentifier));
	}
};

/**
 * 입력 태그에 연결된 단발 어빌리티의 활성화 시도 결과입니다
 * 호출자는 이 값만으로 입력을 보관할지 결정하며, 보관 대상은 FailedToActivate뿐입니다
 */
enum class ERSInputActivationResult : uint8
{
	/** 어빌리티가 이미 실행 중이거나 제거되어 활성화를 시도할 비활성 후보가 없습니다 */
	NoActivationCandidate,

	/** 후보 하나를 활성화했으므로 같은 입력을 다시 처리하지 않습니다 */
	Activated,

	/** 후보는 있었으나 행동 잠금이나 쿨다운으로 모두 실패해 잠시 뒤 다시 시도할 가치가 있습니다 */
	FailedToActivate
};

/** RS의 어빌리티 부여와 활성화, 입력 처리를 담당하는 ASC입니다 */
UCLASS()
class RS_API URSAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	/** Notify State 실행에 대응하는 공용 상태 Lease를 시작합니다 */
	void BeginAnimationGameplayState(UObject* Source, int32 MontageInstanceIdentifier, int32 NotifyInstanceIdentifier, const FGameplayTagContainer& StateTags);

	/** 같은 Notify State 실행이 시작한 공용 상태 Lease를 종료합니다 */
	void EndAnimationGameplayState(UObject* Source, int32 MontageInstanceIdentifier, int32 NotifyInstanceIdentifier);

	/** 중단된 애니메이션 출처가 남긴 공용 게임플레이 상태를 모두 정리합니다 */
	void EndAnimationGameplayStates(UObject* Source);

	/**
	 * 입력 태그와 연결된 어빌리티를 누름 상태로 기록합니다
	 * 어빌리티 활성화는 ProcessAbilityInput이 담당하므로 여기서는 상태만 남깁니다
	 */
	void AbilityInputTagPressed(const FGameplayTag& InputTag);

	/** 입력 태그와 연결된 어빌리티를 해제 상태로 기록합니다 */
	void AbilityInputTagReleased(const FGameplayTag& InputTag);

	/**
	 * 기록된 입력 상태를 활성화 정책에 따라 처리하고 활성화하지 못한 단발 입력을 보관합니다
	 * 입력이 모두 반영된 뒤 실행되어야 하므로 PlayerController의 PostProcessInput에서 매 프레임 호출합니다
	 */
	void ProcessAbilityInput(float DeltaTime, bool bGamePaused);

	/**
	 * 기록된 모든 어빌리티 입력 상태와 보관한 입력을 제거합니다
	 * 사망이나 Avatar 교체처럼 이전 입력을 이어서 처리하면 안 되는 시점에 호출합니다
	 */
	void ClearAbilityInput();

protected:
	/** 실행 중인 어빌리티에 입력 누름 이벤트를 전달합니다 */
	virtual void AbilitySpecInputPressed(FGameplayAbilitySpec& Spec) override;

	/** 실행 중인 어빌리티에 입력 해제 이벤트를 전달합니다 */
	virtual void AbilitySpecInputReleased(FGameplayAbilitySpec& Spec) override;

private:
	/**
	 * 입력 태그와 연결된 OnInputTriggered 어빌리티의 활성화를 시도하고 결과를 반환합니다
	 * Spec Handle이 아니라 태그를 받는 이유는 재시도 시점마다 활성화할 수 있는 Spec이 달라지기 때문이며,
	 * 이번 프레임의 입력과 보관한 입력이 같은 경로를 사용합니다
	 */
	ERSInputActivationResult TryActivateInputTriggeredAbilities(const FGameplayTag& InputTag);

	/**
	 * 보관한 입력의 남은 시간을 갱신하고 활성화를 다시 시도합니다
	 * 새 입력이 보관한 입력을 대체하므로 이번 프레임에 새 어빌리티 입력이 없을 때만 호출합니다
	 */
	void ProcessBufferedAbilityInput(float DeltaTime);

	/** 보관한 입력을 제거합니다 */
	void ClearBufferedAbilityInput();

	/** 애니메이션과 Notify 실행별로 공용 상태 태그의 수명을 소유하는 Effect 핸들입니다 */
	TMap<FRSAnimationGameplayStateKey, FActiveGameplayEffectHandle> AnimationGameplayStateEffectHandles;


	/**
	 * 입력 상태에 따라 어빌리티 스펙 핸들을 분리하여 관리합니다
	 * Pressed와 Released는 해당 프레임의 ProcessAbilityInput에서 처리한 뒤 초기화하고,
	 * Held는 입력을 해제하거나 ClearAbilityInput을 호출할 때까지 유지합니다
	 */

	/** 이번 프레임에 입력이 시작되어 한 번만 처리할 어빌리티입니다 */
	TArray<FGameplayAbilitySpecHandle> InputPressedSpecHandles;

	/** 이번 프레임에 입력이 해제되어 한 번만 처리할 어빌리티입니다 */
	TArray<FGameplayAbilitySpecHandle> InputReleasedSpecHandles;

	/** 입력을 해제할 때까지 유지 상태로 처리할 어빌리티입니다 */
	TArray<FGameplayAbilitySpecHandle> InputHeldSpecHandles;


	/**
	 * 행동 잠금이나 쿨다운으로 활성화하지 못한 단발 입력을 짧은 시간 보관했다가 다시 시도합니다
	 * 매 프레임 갱신되는 Held와 달리 OnInputTriggered 입력은 눌린 프레임을 놓치면 그대로 유실되기 때문입니다
	 * 보관은 하나만 유지하며, 새 어빌리티 입력이 들어오면 활성화 정책과 무관하게 이전 보관을 폐기합니다
	 */

	/** 활성화하지 못한 단발 입력을 보관할 시간입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Input", meta = (AllowPrivateAccess = "true"))
	float InputBufferDuration = 0.2f;

	/**
	 * 이번 프레임에 어빌리티와 연결된 입력이 시작되었는지 나타내며 보관한 입력의 폐기 조건입니다
	 * 활성화 정책까지 따지면 정책마다 폐기 시점이 달라져, 두 입력이 겹칠 때 프레임 내 처리 순서가 결과를 정하게 됩니다
	 */
	bool bAbilityInputPressedThisFrame = false;

	/** 이번 프레임에 가장 마지막으로 입력이 시작된 OnInputTriggered 어빌리티의 입력 태그입니다 */
	FGameplayTag InputTriggeredTag;

	/** 활성화를 기다리는 입력 태그이며 활성화 성공, 후보 소멸, 만료, 새 입력, 입력 초기화 시 제거합니다 */
	FGameplayTag BufferedInputTag;

	/** 보관한 입력이 만료되기까지 남은 시간입니다 */
	float BufferedInputRemainingTime = 0.0f;
};
