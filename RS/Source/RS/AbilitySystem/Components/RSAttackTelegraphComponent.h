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

	/** 나타나는 데 걸릴 시간이며 0이면 즉시 나타납니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Telegraph", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "s"))
	float FadeInDuration = 0.0f;

	/** 사라지는 데 걸릴 시간이며 0이면 즉시 사라집니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Telegraph", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "s"))
	float FadeOutDuration = 0.0f;

	/**
	 * 표시가 채워지는 데 걸릴 시간이며 0이면 채우지 않고 완성 상태로 표시합니다
	 * 판정 순간에 정확히 1이 되도록 이 값을 계산하는 것은 요청자의 책임입니다
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Telegraph", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "s"))
	float FillDuration = 0.0f;
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

	FRSCombatShape Shape;
	FRSTelegraphPresentation Presentation;
	float ElapsedTime = 0.0f;
	bool bIsActive = false;
};

/**
 * 공격 범위를 바닥에 미리 그려 회피를 학습할 수 있게 하는 표시 컴포넌트입니다
 * 형상과 월드 Transform을 받아 그리기만 하며 어떤 형상인지에 따라 동작이 갈리지 않습니다
 * 어빌리티, Montage와 판정을 알지 않으므로 표시를 통째로 제거해도 게임플레이는 동작합니다
 */
UCLASS(ClassGroup = (RS), meta = (BlueprintSpawnableComponent))
class RS_API URSAttackTelegraphComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URSAttackTelegraphComponent();

	/** 활성 표시가 있을 때만 Tick하며 경과 시간을 머티리얼 값에 반영합니다 */
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** 컴포넌트가 제거될 때 남아 있는 데칼을 회수합니다 */
	virtual void OnUnregister() override;

	/**
	 * 형상을 지정한 월드 위치에 표시하기 시작합니다
	 * 유지 시간이 지나면 스스로 사라지므로 정상 흐름에서는 해제를 지시하지 않아도 됩니다
	 */
	UFUNCTION(BlueprintCallable, Category = "RS|Telegraph")
	void ShowShape(const FRSCombatShape& Shape, const FTransform& ShapeTransform, const FRSTelegraphPresentation& Presentation);

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

	/** 페이드 진행도를 전달할 머티리얼 스칼라 파라미터 이름입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Telegraph|Material", meta = (AllowPrivateAccess = "true"))
	FName AlphaParameterName = TEXT("Alpha");

	/** 채움 진행도를 전달할 머티리얼 스칼라 파라미터 이름입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Telegraph|Material", meta = (AllowPrivateAccess = "true"))
	FName FillParameterName = TEXT("Fill");

private:
	/** 데칼과 Dynamic Material Instance를 재사용하기 위한 슬롯 목록입니다 */
	UPROPERTY(Transient)
	TArray<FRSTelegraphSlot> Slots;
};
