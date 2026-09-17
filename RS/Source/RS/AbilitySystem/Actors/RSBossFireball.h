#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "RSAbilitySet.h"
#include "Abilities/Boss/RSBossPatternHitDefinition.h"
#include "RSBossFireball.generated.h"

class UCurveFloat;
class UNiagaraComponent;
class URSAbilitySystemComponent;
class URSAttackTelegraphComponent;
class URSBossPersistentObjectLifetimeComponent;
class URSFireballChargeWidget;
class URSHealthComponent;
class URSHealthSet;
class UBoxComponent;
class USceneComponent;
class UWidgetComponent;
#if WITH_EDITOR
class FDataValidationContext;
#endif

/** 화염구 Actor가 거치는 게임플레이 상태입니다 */
UENUM(BlueprintType)
enum class ERSBossFireballState : uint8
{
	Falling,
	Charging,
	ChargeCompletedPending,
	FireField,
	Cleanup
};

/** 화염구의 낙하, 충전, 장판과 특수 패턴 수명 설정입니다 */
USTRUCT(BlueprintType)
struct FRSBossFireballDefinition
{
	GENERATED_BODY()

	/** 착지점 위에서 낙하 시각 요소가 시작할 높이입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Fireball|Falling", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm"))
	float FallHeight = 1000.0f;

	/** 시각 요소가 착지점까지 내려오는 시간입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Fireball|Falling", meta = (ClampMin = "0.01", UIMin = "0.01", ForceUnits = "s"))
	float FallDuration = 1.0f;

	/** 낙하 진행률을 시각 요소의 높이로 변환하며 비어 있으면 선형으로 이동합니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Fireball|Falling")
	TObjectPtr<UCurveFloat> FallCurve;

	/** 착지 후 장판으로 전환되기 전까지 플레이어가 공격할 수 있는 시간입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Fireball|Charging", meta = (ClampMin = "0.01", UIMin = "0.01", ForceUnits = "s"))
	float ChargeDuration = 3.0f;

	/** 활성 장판의 수평 HitCheck 반경입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Fireball|Fire Field", meta = (ClampMin = "0.01", UIMin = "0.01", ForceUnits = "cm"))
	float FireFieldRadius = 250.0f;

	/** 장판이 자연스럽게 유지되는 시간입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Fireball|Fire Field", meta = (ClampMin = "0.01", UIMin = "0.01", ForceUnits = "s"))
	float FireFieldDuration = 8.0f;

	/** 장판 전환 뒤 첫 피해와 이후 반복 피해 사이의 간격입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Fireball|Fire Field", meta = (ClampMin = "0.01", UIMin = "0.01", ForceUnits = "s"))
	float DamageInterval = 1.0f;

	/**
	 * 장판 구간에서 유지할 Telegraph 불투명도이며 충전 예고보다 낮게 두어 두 표시를 구분합니다
	 * 예고는 차오르며 움직이는 밝은 표시, 장판은 움직이지 않는 흐린 표시가 됩니다
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Fireball|Fire Field", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float FireFieldTelegraphAlpha = 0.35f;

	/** 생성 특수 패턴 이후 몇 번째 특수 패턴에서 이 화염구를 정리할지 나타냅니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Fireball|Lifetime", meta = (ClampMin = "1", UIMin = "1"))
	int32 SpecialPatternCleanupOffset = 2;

	/** 런타임에서 상태와 Timer를 안전하게 구성할 수 있는 값인지 검사합니다 */
	bool IsDataValid(FString* OutValidationError = nullptr) const;

};

/** 낙하, 충전, 피격 횟수와 장판 수명을 하나의 Actor로 관리하는 보스 화염구입니다 */
UCLASS(Blueprintable)
class RS_API ARSBossFireball : public AActor, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ARSBossFireball();

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#if WITH_EDITOR
	/** 화염구 수명, AbilitySet과 표현 에셋의 필수 설정을 검사합니다 */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

#pragma region GAS

public:
	/** 화염구가 직접 소유한 ASC를 반환합니다 */
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

private:
	/** Owner와 Avatar를 화염구 자신으로 초기화하고 기본 AbilitySet을 부여합니다 */
	void InitializeAbilitySystem();

	/** 공용 체력이 0이 되면 장판 전환보다 먼저 화염구를 정리합니다 */
	UFUNCTION()
	void HandleDeathStarted(URSHealthComponent* InHealthComponent);

	/** 피격으로 남은 타격 수가 줄면 충전 위젯의 체력 칸을 갱신합니다 */
	UFUNCTION()
	void HandleHealthChanged(URSHealthComponent* InHealthComponent, float OldValue, float NewValue);

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Abilities", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URSAbilitySystemComponent> AbilitySystemComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Health", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URSHealthSet> HealthSet;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Health", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URSHealthComponent> HealthComp;

	/** 장판 피해 Ability를 포함해 화염구 ASC에 부여할 AbilitySet입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Abilities", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URSAbilitySet> DefaultAbilitySet;

	UPROPERTY(Transient)
	FRSAbilitySet_GrantedHandles GrantedAbilityHandles;

#pragma endregion

#pragma region State

public:
	/** FinishSpawning 전에 생성 Ability가 확정한 게임플레이 설정을 전달합니다 */
	void Initialize(const FRSBossFireballDefinition& InFireballDefinition, const FRSBossPatternHitSpec& InHitSpec);

	/** 현재 화염구 상태를 반환합니다 */
	ERSBossFireballState GetFireballState() const { return FireballState; }

	/** Timer, Widget, Niagara와 ASC 상태를 회수하고 Actor를 제거합니다 */
	void RequestCleanup();

#if WITH_DEV_AUTOMATION_TESTS
	/** 자동 테스트가 충전 예고와 장판 표시가 같은 Handle을 이어 쓰는지 확인합니다 */
	int32 GetTelegraphHandleForTest() const { return TelegraphHandle; }

	/** 자동 테스트가 전역 Frame을 조작하지 않고 장판 전환 이후 상태를 확인합니다 */
	void BeginFireFieldForTest() { BeginFireField(); }
#endif

private:
	void BeginFalling();
	void AdvanceFalling(float DeltaSeconds);
	void BeginCharging();
	void AdvanceCharging(float DeltaSeconds);
	void RequestFireFieldTransition();
	void CompleteFireFieldTransition();
	void BeginFireField();
	void ApplyFireFieldDamage();
	void UpdateChargeWidget(float Progress);
	void UpdateHitPointWidget();

	/** 착지한 자리에 장판 반경과 같은 예고 표시를 시작합니다 */
	void ShowChargeTelegraph();

	/** 충전 진행률을 예고 표시의 채움에 반영합니다 */
	void UpdateChargeTelegraph(float Progress);

	/** 충전 예고에 쓰던 표시를 장판 표시로 전환합니다 */
	void SwitchTelegraphToFireField();

	/** Telegraph Handle만 회수합니다 */
	void HideTelegraph();

	void ClearTimers();

private:
	UPROPERTY(Transient)
	FRSBossFireballDefinition FireballDefinition;

	/** 생성 Ability 레벨에서 확정한 장판 피해와 피격 반응입니다 */
	UPROPERTY(Transient)
	FRSBossPatternHitSpec HitSpec;

	UPROPERTY(Transient)
	ERSBossFireballState FireballState = ERSBossFireballState::Falling;

	float StateElapsedTime = 0.0f;
	int32 TelegraphHandle = INDEX_NONE;
	uint64 ChargeCompletionFrame = MAX_uint64;
	FTimerHandle ChargeCompletionTimerHandle;
	FTimerHandle FireFieldDamageTimerHandle;
	FTimerHandle FireFieldLifetimeTimerHandle;

#pragma endregion

#pragma region Presentation And Collision

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Fireball", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> BoxComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Fireball", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> VisualRoot;

	/** 화염구 본체를 표현하는 Niagara이며 Mesh 없이 이 Component만 화염구를 보여 줍니다. 재생할 System은 Blueprint에서 이 Component의 Asset에 지정합니다 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Fireball", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraComponent> FireballNiagaraComp;

	/** 장판을 표현하는 Niagara이며 재생할 System과 보이는 크기를 Blueprint에서 이 Component의 Asset과 User Parameter로 지정합니다 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Fireball", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraComponent> FireFieldNiagaraComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Fireball", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWidgetComponent> ChargeWidgetComp;

	/**
	 * 충전 예고와 장판 범위를 바닥에 그리는 표시 컴포넌트입니다
	 * 이 Actor는 자신을 만든 패턴보다 오래 살아남으므로 보스의 컴포넌트를 빌리지 않고 직접 소유합니다
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Fireball", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URSAttackTelegraphComponent> TelegraphComp;

	/** 보스 패턴 순번과 공용 정리 이벤트를 이 Actor의 정리 요청으로 변환합니다 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Fireball|Lifetime", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URSBossPersistentObjectLifetimeComponent> PersistentObjectLifetimeComp;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Fireball|Presentation", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<URSFireballChargeWidget> ChargeWidgetClass;

	/** 장판이 노릴 PlayerHurtBox Trace Channel입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Fireball|Hit", meta = (AllowPrivateAccess = "true"))
	TEnumAsByte<ECollisionChannel> PlayerTargetChannel = ECollisionChannel::ECC_GameTraceChannel1;

#pragma endregion
};
