// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RSDamageNumberLayerWidget.generated.h"

class UCanvasPanel;
class URSDamageNumberViewModel;
class URSDamageNumberWidget;

/**
 * 표시 중인 데미지 숫자 하나의 상태입니다
 * 대상 Actor를 참조하지 않으므로 사망 처리와 무관하게 남은 수명을 끝까지 재생합니다
 */
struct FRSActiveDamageNumber
{
	/** 타격 순간에 고정된 월드 앵커이며 수명 동안 변경하지 않습니다 */
	FVector WorldAnchor = FVector::ZeroVector;

	/** 활성화된 이후 흐른 시간입니다 */
	float ElapsedSeconds = 0.0f;

	/** 동시 타격이 겹치지 않도록 적용하는 Widget 좌표 수평 오프셋입니다 */
	float HorizontalScatter = 0.0f;

	/** 이 항목이 사용 중인 표시 위젯입니다 */
	TObjectPtr<URSDamageNumberWidget> DamageNumberWidget;
};

/**
 * 데미지 숫자의 투영과 수명, 위젯 풀을 관리하는 전체 화면 계층입니다
 * 게임 객체를 조회하지 않고 ViewModel의 요청만 소비합니다
 */
UCLASS(Abstract)
class RS_API URSDamageNumberLayerWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

protected:
	/** 표시에 사용할 데미지 숫자 위젯 클래스입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage Number")
	TSubclassOf<URSDamageNumberWidget> DamageNumberWidgetClass;

	/** 동시에 표시할 수 있는 데미지 숫자의 최대 개수이며 초과하면 가장 오래된 항목을 회수합니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage Number", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxConcurrentNumbers = 12;

	/** 데미지 숫자 하나가 표시되는 시간입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage Number", meta = (ClampMin = "0.01", UIMin = "0.01", Units = "s"))
	float LifetimeSeconds = 0.8f;

	/** 수명 동안 떠오를 Widget 좌표 거리이며 월드 거리가 아닙니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage Number", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float RiseDistance = 60.0f;

	/** 동시 타격을 흩어 놓을 Widget 좌표 수평 범위입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage Number", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float HorizontalScatterRange = 24.0f;

private:
	/** MVVM Context Source로 해석된 데미지 숫자 ViewModel의 요청을 구독합니다 */
	void BindToDamageNumberViewModel();

	/** 구독을 해제합니다 */
	void UnbindFromDamageNumberViewModel();

	/** 요청 하나를 받아 풀에서 항목을 활성화합니다 */
	UFUNCTION()
	void HandleDamageNumberRequested(FText DisplayDamageText, FVector WorldAnchor);

	/** 표시 중인 모든 항목을 즉시 회수합니다 */
	UFUNCTION()
	void HandleDamageNumberResetRequested();

	/**
	 * 재사용할 위젯을 확보합니다
	 * 상한에 닿으면 가장 오래된 활성 항목을 회수하므로 위젯 개수는 상한을 넘지 않습니다
	 */
	URSDamageNumberWidget* AcquireDamageNumberWidget();

	/** 활성 항목을 정리하고 위젯을 풀로 되돌립니다 */
	void ReleaseActiveDamageNumber(int32 ActiveIndex);

	/** 고정 앵커를 다시 투영하고 상승과 페이드를 적용합니다 */
	void UpdateActiveDamageNumber(FRSActiveDamageNumber& ActiveDamageNumber);

private:
	/**
	 * 데미지 숫자를 배치하는 전체 화면 캔버스이며 이 위젯의 루트입니다
	 * 좌표계 계약이 캔버스가 뷰포트 전체와 일치할 것을 요구하므로 이름으로 찾지 않고 루트를 사용합니다
	 */
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvasPanel;

	/** 요청을 전달하는 ViewModel입니다 */
	UPROPERTY(Transient)
	TWeakObjectPtr<URSDamageNumberViewModel> BoundDamageNumberViewModel;

	/** 현재 표시 중인 항목이며 앞쪽이 더 오래된 항목입니다 */
	TArray<FRSActiveDamageNumber> ActiveDamageNumbers;

	/** 회수되어 재사용을 기다리는 위젯입니다 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<URSDamageNumberWidget>> DamageNumberWidgetPool;

	/** 지금까지 만든 표시 위젯의 수이며 상한 판정에 사용합니다 */
	int32 CreatedDamageNumberWidgetCount = 0;
};
