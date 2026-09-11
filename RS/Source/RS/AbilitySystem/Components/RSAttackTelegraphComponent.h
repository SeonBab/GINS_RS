// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/RSCombatFunctionLibrary.h"
#include "RSAttackTelegraphComponent.generated.h"

class UDecalComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;

/**
 * 표시 하나의 시간 표현이며 형상과 위치는 담지 않습니다
 * 공격마다 다른 값이므로 컴포넌트가 아니라 요청이 소유합니다
 */
USTRUCT(BlueprintType)
struct FRSTelegraphPresentation
{
	GENERATED_BODY()

	/** 표시를 유지할 시간이며 이 시간이 지나면 스스로 사라집니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Telegraph", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "s"))
	float HoldDuration = 1.0f;

	/**
	 * 표시가 채워지는 데 걸릴 시간이며 0이면 채우지 않고 완성 상태로 표시합니다
	 * 판정 순간에 정확히 1이 되도록 이 값을 계산하는 것은 요청자의 책임입니다
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Telegraph", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "s"))
	float FillDuration = 0.0f;
};

/**
 * 판정 시각을 기준으로 Fill 완료와 표시 소멸이 각각 얼마나 빠른지를 저술하는 값입니다
 * 판정 시각을 아는 쪽은 요청 Ability뿐이므로 표시 컴포넌트는 이 값을 읽지 않습니다
 * Fill은 항상 끝까지 진행하므로 Fill 완료, 표시 소멸, 판정 순서가 언제나 유지됩니다
 */
USTRUCT(BlueprintType)
struct FRSTelegraphLeadTime
{
	GENERATED_BODY()

	/** Fill이 판정보다 이만큼 먼저 1에 도달하며 플레이어가 완성된 범위를 읽을 시간입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Telegraph", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "s"))
	float FillLeadTime = 0.15f;

	/**
	 * 표시가 판정보다 이만큼 먼저 사라지며 판정 순간에는 바닥에 표시가 남지 않습니다
	 * 표시가 사라진 뒤 판정까지의 빈 구간이므로 길어지면 예고가 거짓말처럼 읽힙니다
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Telegraph", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "s"))
	float HideLeadTime = 0.05f;

	/** 표시 시작부터 Fill이 1에 도달하기까지의 시간이며 소멸보다 늦어지지 않습니다 */
	float GetFillDuration(float ImpactDelay) const;

	/** 표시 시작부터 표시가 사라지기까지의 시간입니다 */
	float GetHideTime(float ImpactDelay) const;

	/** 시간 기반 표시가 읽는 두 절대 시간으로 변환합니다 */
	FRSTelegraphPresentation MakePresentation(float ImpactDelay) const;

	/** 세 시점이 Fill 완료, 표시 소멸, 판정 순서를 지킬 수 있는 값인지 검사합니다 */
	bool IsDataValid(float ImpactDelay, FString* OutValidationError = nullptr) const;
};

/** 외부 진행률로 채우는 환형 부채꼴 Telegraph의 공간 데이터입니다 */
USTRUCT(BlueprintType)
struct FRSAnnularSectorTelegraphDefinition
{
	GENERATED_BODY()

	/** 공격 Pivot에서 경로 안쪽까지의 거리입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Telegraph", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm"))
	float InnerRadius = 0.0f;

	/** 공격 Pivot에서 경로 바깥쪽까지의 거리입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Telegraph", meta = (ClampMin = "0.0", UIMin = "1.0", ForceUnits = "cm"))
	float OuterRadius = 0.0f;

	/** 고정된 기준 Yaw에서 경로가 시작되는 상대 각도입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Telegraph", meta = (ForceUnits = "deg"))
	float StartYawOffset = 0.0f;

	/** 경로 전체 각도이며 부호가 채워지는 방향을 결정합니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Telegraph", meta = (ClampMin = "-360.0", ClampMax = "360.0", ForceUnits = "deg"))
	float SweepAngleDegrees = 0.0f;

	/** 환형 부채꼴을 표시할 수 있는 값인지 검사합니다 */
	bool IsDataValid() const;
};

/** 표시 하나의 형상, 위치, 시간과 데칼 수명을 함께 보관합니다 */
USTRUCT()
struct FRSTelegraphSlot
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UDecalComponent> Decal;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MaterialInstance;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> SourceMaterial;

	FRSCombatShape Shape;
	FRSTelegraphPresentation Presentation;
	float ElapsedTime = 0.0f;
	int32 Handle = INDEX_NONE;
	bool bIsActive = false;
	bool bUsesExternalFill = false;
	float ProjectionYawOffset = 0.0f;
};

/**
 * 공격 범위를 바닥에 미리 그려 회피를 학습할 수 있게 하는 표시 컴포넌트입니다
 * 고정 Shape의 시간 기반 표시와 환형 부채꼴의 외부 진행률 표시를 소유합니다
 * 어빌리티, Montage와 판정을 알지 않으므로 표시를 제거해도 게임플레이 판정은 변하지 않습니다
 */
UCLASS(ClassGroup = (RS), meta = (BlueprintSpawnableComponent))
class RS_API URSAttackTelegraphComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URSAttackTelegraphComponent();

	/** 시간 기반 표시가 있을 때만 Tick하며 경과 시간을 머티리얼 값에 반영합니다 */
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** 컴포넌트가 제거될 때 남아 있는 데칼을 회수합니다 */
	virtual void OnUnregister() override;

	/**
	 * 형상을 지정한 월드 위치에 표시하기 시작합니다
	 * 유지 시간이 지나면 스스로 사라지므로 정상 흐름에서는 해제를 지시하지 않아도 됩니다
	 */
	UFUNCTION(BlueprintCallable, Category = "RS|Telegraph")
	void ShowShape(const FRSCombatShape& Shape, const FTransform& ShapeTransform, const FRSTelegraphPresentation& Presentation);

	/** 형상을 표시하고 해당 표시만 조기에 회수할 수 있는 Handle을 반환합니다 */
	UFUNCTION(BlueprintCallable, Category = "RS|Telegraph")
	int32 ShowShapeWithHandle(const FRSCombatShape& Shape, const FTransform& ShapeTransform, const FRSTelegraphPresentation& Presentation);

	/** 형상을 월드에 고정하고 외부 진행률 0으로 표시합니다 */
	int32 ShowShapeWithExternalFill(const FRSCombatShape& Shape, const FTransform& ShapeTransform);

	/**
	 * 환형 부채꼴을 월드에 고정하고 외부 진행률 0으로 표시합니다
	 * 공용 Material이 없거나 공간 데이터가 잘못되면 대체 표시 없이 실패합니다
	 */
	int32 ShowAnnularSector(const FRSAnnularSectorTelegraphDefinition& Definition, const FTransform& LockedTransform);

	/** 외부 수명 표시의 채움 진행률을 갱신합니다 */
	bool SetExternalFill(int32 Handle, float Fill);

	/** 표시의 고정 형상은 유지하고 월드 Transform만 갱신합니다 */
	bool SetShapeTransform(int32 Handle, const FTransform& ShapeTransform);

	/** 지정한 표시를 즉시 회수합니다 */
	void HideShape(int32 Handle);

	/**
	 * 표시 중인 모든 형상을 페이드 없이 즉시 회수합니다
	 * 공격이 취소되거나 소유자가 사망할 때처럼 표시를 남기면 안 되는 경로가 호출합니다
	 */
	UFUNCTION(BlueprintCallable, Category = "RS|Telegraph")
	void HideAllShapes();

private:
	/** 비활성 슬롯을 재사용하거나 없으면 새로 만들어 반환합니다 */
	FRSTelegraphSlot& AcquireSlot();

	/** 슬롯의 데칼을 형상과 위치에 맞게 구성합니다 */
	void SetUpSlotDecal(FRSTelegraphSlot& Slot, const FTransform& ShapeTransform);

	/** 슬롯의 데칼을 환형 부채꼴과 고정 Transform에 맞게 구성합니다 */
	bool SetUpAnnularSectorDecal(FRSTelegraphSlot& Slot, const FRSAnnularSectorTelegraphDefinition& Definition, const FTransform& LockedTransform);

	/** 슬롯이 요청한 원본 Material의 Dynamic Material Instance를 사용하게 합니다 */
	bool SetUpSlotMaterial(FRSTelegraphSlot& Slot, UMaterialInterface* Material);

	/** 활성 Handle이 가리키는 슬롯을 반환합니다 */
	FRSTelegraphSlot* FindSlot(int32 Handle);

	/** 외부에서 보관할 수 있는 새 표시 Handle을 반환합니다 */
	int32 CreateHandle();

	/** 경과 시간으로 슬롯의 머티리얼 값을 갱신하고 수명이 끝났으면 회수합니다 */
	void UpdateSlot(FRSTelegraphSlot& Slot, float DeltaTime);

	/** 슬롯을 비활성으로 되돌리고 데칼을 숨깁니다 */
	void ReleaseSlot(FRSTelegraphSlot& Slot);

	/** 활성 슬롯 유무에 맞춰 Tick을 켜고 끕니다 */
	void RefreshTickEnabled();

private:
	/** 데칼에 사용할 머티리얼이며 지정하지 않으면 개발용 디버그 드로우로 대신합니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Telegraph", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMaterialInterface> DecalMaterial;

	/**
	 * 데칼이 바닥을 향해 투영할 깊이의 반값입니다
	 * 볼륨이 원점 기준 대칭이라 이 값만큼 위로도 뻗으므로 벽과 소품의 밑단이 물들지 않을 만큼 얇게 둡니다
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Telegraph", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0", ForceUnits = "cm"))
	float ProjectionDepth = 20.0f;

	/** 도넛의 안쪽 비율을 전달할 머티리얼 스칼라 파라미터 이름입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Telegraph|Material", meta = (AllowPrivateAccess = "true"))
	FName InnerRatioParameterName = TEXT("InnerRatio");

	/** 요청별 고정 불투명도를 전달할 머티리얼 스칼라 파라미터 이름입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Telegraph|Material", meta = (AllowPrivateAccess = "true"))
	FName AlphaParameterName = TEXT("Alpha");

	/** 채움 진행도를 전달할 머티리얼 스칼라 파라미터 이름입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Telegraph|Material", meta = (AllowPrivateAccess = "true"))
	FName FillParameterName = TEXT("Fill");

	/** 원·링, Cone과 환형 부채꼴 중 어느 형상 경로를 사용할지 전달할 스칼라 파라미터 이름입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Telegraph|Material", meta = (AllowPrivateAccess = "true"))
	FName ShapeModeParameterName = TEXT("ShapeMode");

	/** 반경 방향과 각도 방향 중 어느 채움 경로를 사용할지 전달할 스칼라 파라미터 이름입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Telegraph|Material", meta = (AllowPrivateAccess = "true"))
	FName FillModeParameterName = TEXT("FillMode");

	/** Cone 전체 각도의 절반에 대한 Cos 값을 전달할 스칼라 파라미터 이름입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Telegraph|Material", meta = (AllowPrivateAccess = "true"))
	FName ConeHalfAngleCosParameterName = TEXT("ConeHalfAngleCos");

	/** 환형 부채꼴의 부호 있는 전체 각도를 전달할 머티리얼 스칼라 파라미터 이름입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Telegraph|Material", meta = (AllowPrivateAccess = "true"))
	FName SweepAngleDegreesParameterName = TEXT("SweepAngleDegrees");

private:
	/** 데칼과 Dynamic Material Instance를 재사용하기 위한 슬롯 목록입니다 */
	UPROPERTY(Transient)
	TArray<FRSTelegraphSlot> Slots;

	/** INDEX_NONE과 충돌하지 않는 다음 외부 표시 Handle입니다 */
	int32 NextHandle = 1;
};
