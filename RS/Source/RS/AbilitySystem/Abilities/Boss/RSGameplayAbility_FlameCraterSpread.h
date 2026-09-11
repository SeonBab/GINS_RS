#pragma once

#include "CoreMinimal.h"
#include "RSBaseGameplayAbility_BossPattern.h"
#include "RSGameplayAbility_FlameCraterSpread.generated.h"

class ARSBossFlameCrater;
class UAnimMontage;

/** 화염 분화구 네 개를 플레이어 방향의 180도 절반에 분산 배치하는 공간 설정입니다 */
USTRUCT(BlueprintType)
struct FRSFlameCraterSpreadDefinition
{
	GENERATED_BODY()

	/** 한 번의 패턴에서 생성할 화염 분화구 수이며 180도를 네 개의 45도 구역으로 나눕니다 */
	static constexpr int32 FlameCraterCount = 4;

	/** 보스 바닥 중심에서 화염 분화구 중심까지의 최소 수평 거리입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|FlameCrater Spread|Placement", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm"))
	float MinSpawnRadius = 250.0f;

	/** 보스 바닥 중심에서 화염 분화구 중심까지의 최대 수평 거리입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|FlameCrater Spread|Placement", meta = (ClampMin = "0.01", UIMin = "0.01", ForceUnits = "cm"))
	float MaxSpawnRadius = 1000.0f;

	/** 화염 분화구 하나가 바닥에서 차지하는 반경이며 경계와 상호 겹침 계산에 사용합니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|FlameCrater Spread|Placement", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm"))
	float PlacementRadius = 50.0f;

	/** 두 화염 분화구 외곽 사이에 확보할 최소 거리입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|FlameCrater Spread|Placement", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm"))
	float MinimumGap = 50.0f;

	/** 화염 분화구 외곽과 담당 구역·생성 반경 경계 사이에 추가할 여유입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|FlameCrater Spread|Placement", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm"))
	float BoundaryMargin = 20.0f;

	/** 모든 45도 구역에서 폴백 없이 한 점을 뽑을 수 있는 설정인지 검사합니다 */
	bool IsDataValid(FString* OutValidationError = nullptr) const;

	/** 각도 구역마다 하나씩 면적 균일 반지름으로 월드 착지 위치를 생성합니다 */
	bool TryGenerateLandingLocations(const FVector& Center, const FVector& HorizontalForward, FRandomStream& InOutRandomStream, TArray<FVector>& OutLocations) const;
};

/** 플레이어 방향의 180도를 네 구역으로 나눠 화염 분화구 네 개를 동시에 투하하는 특수 패턴입니다 */
UCLASS(Abstract, Blueprintable)
class RS_API URSGameplayAbility_FlameCraterSpread : public URSBaseGameplayAbility_BossPattern
{
	GENERATED_BODY()

public:
	URSGameplayAbility_FlameCraterSpread();

	/**
	 * 이 패턴은 파훼 판정을 두지 않습니다
	 * 분화구를 배치하기만 하고 피해는 장판이 FireFieldDamage라는 별개의 어빌리티로 적용하므로
	 * 자기 실행 중에 누가 판정에 걸렸는지 알 수 없습니다
	 */
	virtual bool HasGimmickBreakCondition() const override { return false; }

protected:
	/** 플레이어 방향과 네 착지점을 한 번 확정하고 포효·투하 대기를 시작합니다 */
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	/** 취소된 포효 Montage의 공용 상태를 회수합니다 */
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

#if WITH_EDITOR
	/** 공간 설정, FlameCrater Class와 투하 지연시간을 검사합니다 */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	/** 보스 바닥 중심과 활성화 순간의 플레이어 방향으로 네 착지점을 계산합니다 */
	bool CaptureLandingLocations(const FGameplayAbilityActorInfo* ActorInfo);

	/** 같은 호출에서 네 화염 분화구 Actor를 생성해 동시에 낙하를 시작합니다 */
	bool SpawnFlameCraters();

	/** 투하 지연이 끝나면 화염 분화구를 생성합니다 */
	UFUNCTION()
	void HandleDropDelayFinished();

	/** 포효 Montage가 정상적으로 끝났음을 기록합니다 */
	UFUNCTION()
	void HandleRoarMontageFinished();

	/** 포효 Montage가 중단되면 패턴을 취소합니다 */
	UFUNCTION()
	void HandleRoarMontageCancelled();

	/** 투하와 포효가 모두 끝나면 Ability를 정상 종료합니다 */
	void TryFinishAbility();

private:
	/** 네 화염 분화구의 반경과 경계·간격 설정입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|FlameCrater Spread", meta = (AllowPrivateAccess = "true"))
	FRSFlameCraterSpreadDefinition SpreadDefinition;

	/** 네 착지 위치에 생성할 화염 분화구 Actor Class입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|FlameCrater Spread", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<ARSBossFlameCrater> FlameCraterClass;

	/** 화염 분화구 투하 전에 재생할 선택적 포효 Montage입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|FlameCrater Spread|Presentation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimMontage> RoarMontage;

	/** Ability 활성화 후 네 화염 분화구를 동시에 생성하기까지의 시간입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|FlameCrater Spread|Timing", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0", ForceUnits = "s"))
	float DropDelay = 0.5f;

	/** 활성화 순간 확정한 네 화염 분화구 착지 위치입니다 */
	UPROPERTY(Transient)
	TArray<FVector> ActiveLandingLocations;

	FRandomStream RandomStream;
	bool bDropFinished = false;
	bool bRoarMontageFinished = false;
};
