#pragma once

#include "CoreMinimal.h"
#include "RSPizzaMemoryPatternDefinition.generated.h"

/** 8개 조각에서 서로 반대편인 두 조각으로 구성한 안전지대 쌍입니다 */
UENUM(BlueprintType)
enum class ERSPizzaMemorySafePair : uint8
{
	Slice0And4 UMETA(DisplayName = "Slice 0 and 4"),
	Slice1And5 UMETA(DisplayName = "Slice 1 and 5"),
	Slice2And6 UMETA(DisplayName = "Slice 2 and 6"),
	Slice3And7 UMETA(DisplayName = "Slice 3 and 7")
};

/** 한 번의 피자 암기 패턴에서 순서대로 사용할 안전지대 쌍 목록입니다 */
USTRUCT(BlueprintType)
struct FRSPizzaMemorySafeZoneSequence
{
	GENERATED_BODY()

	/** 같은 안전지대의 비연속 반복과 연속 반복을 모두 허용합니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Pizza Memory Pattern")
	TArray<ERSPizzaMemorySafePair> SafePairs;
};

/** 피자 암기 패턴이 균등 무작위로 선택할 안전지대 순서 후보 설정입니다 */
USTRUCT(BlueprintType)
struct FRSPizzaMemoryPatternDefinition
{
	GENERATED_BODY()

	FRSPizzaMemoryPatternDefinition();

	/** 패턴이 항상 사용하는 전체 조각 수입니다 */
	static constexpr int32 SliceCount = 8;

	/** 선택 가능한 반대편 안전지대 쌍의 수입니다 */
	static constexpr int32 SafePairCount = 4;

	/** 활성화마다 같은 확률로 하나를 선택할 안전지대 순서 후보입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Pizza Memory Pattern")
	TArray<FRSPizzaMemorySafeZoneSequence> SafeZoneSequenceCandidates;

	/** 패턴 중심에서 모든 조각의 외곽까지 이어지는 거리입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Pizza Memory Pattern|Space", meta = (ClampMin = "0.01", UIMin = "0.01", ForceUnits = "cm"))
	float OuterRadius = 1000.0f;

	/** Ability 활성화 뒤 패턴 Transform과 후보를 확정하고 첫 암기 표시를 시작할 때까지의 지연입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Pizza Memory Pattern|Timing", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "s"))
	float AttackStartDelay = 0.0f;

	/** 한 암기 단계에서 위험한 여섯 조각을 고정 표시할 시간입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Pizza Memory Pattern|Timing", meta = (ClampMin = "0.01", UIMin = "0.01", ForceUnits = "s"))
	float MemoryCueDuration = 0.5f;

	/** 마지막 단계를 제외한 암기 표시 사이에 아무것도 보여 주지 않을 시간입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Pizza Memory Pattern|Timing", meta = (ClampMin = "0.01", UIMin = "0.01", ForceUnits = "s"))
	float MemoryCueGap = 0.2f;

	/** 마지막 암기 표시를 지운 뒤 첫 폭발까지 플레이어가 기억한 위치로 이동할 시간입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Pizza Memory Pattern|Timing", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "s"))
	float RecallDelay = 1.0f;

	/** 첫 폭발 이후 다음 폭발을 실행할 때까지의 공통 간격입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Pizza Memory Pattern|Timing", meta = (ClampMin = "0.01", UIMin = "0.01", ForceUnits = "s"))
	float ExplosionInterval = 1.0f;

	/** 후보 목록, 공간과 시간 설정이 런타임에서 사용할 수 있는지 검사합니다 */
	bool IsDataValid(FString* OutValidationError = nullptr) const;

	/** 유효한 후보 목록의 공통 길이를 반환하며 후보가 없으면 0입니다 */
	int32 GetSequenceLength() const;

	/** 안전지대 쌍에 대응하는 두 조각 인덱스를 반환합니다 */
	static bool TryGetSafeSliceIndices(ERSPizzaMemorySafePair SafePair, int32& OutFirstSliceIndex, int32& OutSecondSliceIndex);
};
