#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "RSAbilitySet.h"
#include "RSBossFlameCrater.generated.h"

class UCurveFloat;
class UNiagaraComponent;
class UNiagaraSystem;
class URSAbilitySystemComponent;
class URSBossPhaseComponent;
class URSFlameCraterChargeWidget;
class URSHealthComponent;
class URSHealthSet;
class UCapsuleComponent;
class USceneComponent;
class UStaticMeshComponent;
class UWidgetComponent;
#if WITH_EDITOR
class FDataValidationContext;
#endif

/** 화염 분화구 Actor가 거치는 게임플레이 상태입니다 */
UENUM(BlueprintType)
enum class ERSBossFlameCraterState : uint8
{
	Falling,
	Charging,
	ChargeCompletedPending,
	FireField,
	Cleanup
};

/** 화염 분화구의 낙하, 충전, 장판과 특수 패턴 수명 설정입니다 */
USTRUCT(BlueprintType)
struct FRSBossFlameCraterDefinition
{
	GENERATED_BODY()

	/** 착지점 위에서 낙하 시각 요소가 시작할 높이입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|FlameCrater|Falling", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm"))
	float FallHeight = 1000.0f;

	/** 시각 요소가 착지점까지 내려오는 시간입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|FlameCrater|Falling", meta = (ClampMin = "0.01", UIMin = "0.01", ForceUnits = "s"))
	float FallDuration = 1.0f;

	/** 낙하 진행률을 시각 요소의 높이로 변환하며 비어 있으면 선형으로 이동합니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|FlameCrater|Falling")
	TObjectPtr<UCurveFloat> FallCurve;

	/** 착지 후 장판으로 전환되기 전까지 플레이어가 공격할 수 있는 시간입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|FlameCrater|Charging", meta = (ClampMin = "0.01", UIMin = "0.01", ForceUnits = "s"))
	float ChargeDuration = 3.0f;

	/** 화염 분화구를 파괴하는 데 필요한 개별 타격 수입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|FlameCrater|Charging", meta = (ClampMin = "1", UIMin = "1"))
	int32 RequiredHitCount = 3;

	/** 활성 장판의 표시와 수평 HitCheck가 공유하는 반경입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|FlameCrater|Fire Field", meta = (ClampMin = "0.01", UIMin = "0.01", ForceUnits = "cm"))
	float FireFieldRadius = 250.0f;

	/** 장판이 자연스럽게 유지되는 시간입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|FlameCrater|Fire Field", meta = (ClampMin = "0.01", UIMin = "0.01", ForceUnits = "s"))
	float FireFieldDuration = 8.0f;

	/** 장판 전환 뒤 첫 피해와 이후 반복 피해 사이의 간격입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|FlameCrater|Fire Field", meta = (ClampMin = "0.01", UIMin = "0.01", ForceUnits = "s"))
	float DamageInterval = 1.0f;

	/** 생성 특수 패턴 이후 몇 번째 특수 패턴에서 이 화염 분화구를 정리할지 나타냅니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|FlameCrater|Lifetime", meta = (ClampMin = "1", UIMin = "1"))
	int32 SpecialPatternCleanupOffset = 2;

	/** 런타임에서 상태와 Timer를 안전하게 구성할 수 있는 값인지 검사합니다 */
	bool IsDataValid(FString* OutValidationError = nullptr) const;

	/** 생성 순번으로부터 설정된 특수 패턴 횟수가 지났는지 판정합니다 */
	bool ShouldCleanupForSpecialPattern(int32 SpawnSequence, int32 ActiveSequence) const;
};

/** 낙하, 충전, 피격 횟수와 장판 수명을 하나의 Actor로 관리하는 보스 화염 분화구입니다 */
UCLASS(Blueprintable)
class RS_API ARSBossFlameCrater : public AActor, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ARSBossFlameCrater();

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#if WITH_EDITOR
	/** 화염 분화구 수명, AbilitySet과 표현 에셋의 필수 설정을 검사합니다 */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

#pragma region GAS

public:
	/** 화염 분화구가 직접 소유한 ASC를 반환합니다 */
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	/** 화염 분화구가 사용하는 공용 HealthSet을 반환합니다 */
	const URSHealthSet* GetHealthSet() const { return HealthSet; }

private:
	/** Owner와 Avatar를 화염 분화구 자신으로 초기화하고 기본 AbilitySet을 부여합니다 */
	void InitializeAbilitySystem();

	/** 공용 체력이 0이 되면 장판 전환보다 먼저 화염 분화구를 정리합니다 */
	UFUNCTION()
	void HandleDeathStarted(URSHealthComponent* InHealthComponent);

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Abilities", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URSAbilitySystemComponent> AbilitySystemComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Health", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URSHealthSet> HealthSet;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Health", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URSHealthComponent> HealthComp;

	/** 장판 피해 Ability를 포함해 화염 분화구 ASC에 부여할 AbilitySet입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Abilities", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URSAbilitySet> DefaultAbilitySet;

	UPROPERTY(Transient)
	FRSAbilitySet_GrantedHandles GrantedAbilityHandles;

#pragma endregion

#pragma region State

public:
	/** 현재 화염 분화구 상태를 반환합니다 */
	ERSBossFlameCraterState GetFlameCraterState() const { return FlameCraterState; }

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
	void ClearTimers();

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|FlameCrater", meta = (AllowPrivateAccess = "true"))
	FRSBossFlameCraterDefinition FlameCraterDefinition;

	UPROPERTY(Transient)
	ERSBossFlameCraterState FlameCraterState = ERSBossFlameCraterState::Falling;

	float StateElapsedTime = 0.0f;
	uint64 ChargeCompletionFrame = MAX_uint64;
	FTimerHandle ChargeCompletionTimerHandle;
	FTimerHandle FireFieldDamageTimerHandle;
	FTimerHandle FireFieldLifetimeTimerHandle;

#pragma endregion

#pragma region Pattern Lifetime

private:
	void BindPatternLifetime();
	void UnbindPatternLifetime();
	void HandleSpecialPatternActivationRequested(int32 ActiveSequence);
	void HandlePersistentObjectCleanupRequested();

private:
	TWeakObjectPtr<URSBossPhaseComponent> BossPhaseComp;
	FDelegateHandle SpecialPatternActivationRequestedDelegateHandle;
	FDelegateHandle PersistentObjectCleanupDelegateHandle;
	int32 SpawnSpecialPatternSequence = 0;

#pragma endregion

#pragma region Presentation And Collision

private:
	/** 장판 판정 반경을 기준으로 Niagara Component의 수평 Scale을 갱신합니다 */
	void UpdateFireFieldNiagaraScale();

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|FlameCrater", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCapsuleComponent> CapsuleComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|FlameCrater", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> VisualRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|FlameCrater", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> FlameCraterMeshComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|FlameCrater", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraComponent> FallingNiagaraComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|FlameCrater", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraComponent> FireFieldNiagaraComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|FlameCrater", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWidgetComponent> ChargeWidgetComp;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|FlameCrater|Presentation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraSystem> FallingNiagaraSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|FlameCrater|Presentation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraSystem> FireFieldNiagaraSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|FlameCrater|Presentation", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<URSFlameCraterChargeWidget> ChargeWidgetClass;

	/** Niagara System을 XY Scale 1로 재생했을 때 표현하는 기준 반경입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|FlameCrater|Presentation", meta = (AllowPrivateAccess = "true", ClampMin = "0.01", UIMin = "0.01", ForceUnits = "cm"))
	float FireFieldNiagaraBaseRadius = 100.0f;

	/** 장판이 노릴 PlayerHurtBox Trace Channel입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|FlameCrater|Hit", meta = (AllowPrivateAccess = "true"))
	TEnumAsByte<ECollisionChannel> PlayerTargetChannel = ECollisionChannel::ECC_GameTraceChannel1;

#pragma endregion
};
