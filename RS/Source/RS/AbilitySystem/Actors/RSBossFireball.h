#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "RSAbilitySet.h"
#include "RSBossFireball.generated.h"

class UCurveFloat;
class UNiagaraComponent;
class URSAbilitySystemComponent;
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

	/** 화염구를 파괴하는 데 필요한 개별 타격 수입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Fireball|Charging", meta = (ClampMin = "1", UIMin = "1"))
	int32 RequiredHitCount = 3;

	/** 활성 장판의 수평 HitCheck 반경입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Fireball|Fire Field", meta = (ClampMin = "0.01", UIMin = "0.01", ForceUnits = "cm"))
	float FireFieldRadius = 250.0f;

	/** 장판이 자연스럽게 유지되는 시간입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Fireball|Fire Field", meta = (ClampMin = "0.01", UIMin = "0.01", ForceUnits = "s"))
	float FireFieldDuration = 8.0f;

	/** 장판 전환 뒤 첫 피해와 이후 반복 피해 사이의 간격입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Fireball|Fire Field", meta = (ClampMin = "0.01", UIMin = "0.01", ForceUnits = "s"))
	float DamageInterval = 1.0f;

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
	/** 현재 화염구 상태를 반환합니다 */
	ERSBossFireballState GetFireballState() const { return FireballState; }

	/** Timer, Widget, Niagara와 ASC 상태를 회수하고 Actor를 제거합니다 */
	void RequestCleanup();

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
	void ClearTimers();

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Fireball", meta = (AllowPrivateAccess = "true"))
	FRSBossFireballDefinition FireballDefinition;

	UPROPERTY(Transient)
	ERSBossFireballState FireballState = ERSBossFireballState::Falling;

	float StateElapsedTime = 0.0f;
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
