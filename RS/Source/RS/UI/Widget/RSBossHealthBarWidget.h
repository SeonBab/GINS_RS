// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FieldNotificationId.h"
#include "RSBossHealthBarWidget.generated.h"

class UProgressBar;
class URSBossStatusViewModel;

/**
 * 보스 체력바에 짧은 절차적 피격 흔들림과 깎인 체력의 반투명 잔상 하강을 적용하는 위젯입니다
 * 지속 체력 상태는 MVVM FieldNotify가 담당하고 이 클래스는 표현에만 쓰는 상태를 처리합니다
 */
UCLASS(Abstract)
class RS_API URSBossHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 기존 흔들림을 원점으로 되돌린 뒤 전달된 강도로 처음부터 재생합니다 */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "RS|Boss Health Bar")
	void RequestHealthBarShake(float Strength);

	/** 진행 중인 흔들림을 중단하고 Render Translation을 정확히 원점으로 복원합니다 */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "RS|Boss Health Bar")
	void ResetHealthBarShake();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Strength 1일 때 수평 최대 진폭입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shake", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MaxShakeX = 8.0f;

	/** Strength 1일 때 수직 최대 진폭입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shake", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MaxShakeY = 2.0f;

	/** Strength와 무관한 고정 재생 시간입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shake", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float ShakeDuration = 0.15f;

	/** 첫 번째 활성 레이어의 색입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Layer Colors")
	FLinearColor RedLayerColor = FLinearColor(0.85f, 0.08f, 0.08f, 1.0f);

	/** 두 번째 활성 레이어의 색입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Layer Colors")
	FLinearColor OrangeLayerColor = FLinearColor(0.91f, 0.32f, 0.05f, 1.0f);

	/** 세 번째 활성 레이어의 색입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Layer Colors")
	FLinearColor YellowLayerColor = FLinearColor(0.85f, 0.65f, 0.03f, 1.0f);

	/** 네 번째 활성 레이어의 색입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Layer Colors")
	FLinearColor GreenLayerColor = FLinearColor(0.12f, 0.62f, 0.20f, 1.0f);

	/** 다음 레이어가 없을 때 빈 영역에 사용하는 색입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Layer Colors")
	FLinearColor EmptyLayerColor = FLinearColor(0.04f, 0.04f, 0.04f, 1.0f);

	/** 아직 줄어드는 중인 깎인 체력 구간의 색이며 실제 체력과 구분되도록 반투명하게 사용합니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Delayed Drain")
	FLinearColor ChipColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.35f);

	/** 실제 체력 바가 목표를 따라가는 속도의 기준이며 잔상보다 빨라야 깎인 구간이 드러납니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Delayed Drain", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float DisplayInterpSpeed = 12.0f;

	/** 잔상 하강 속도의 기준이며 값이 클수록 같은 피해량을 더 빨리 따라잡습니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Delayed Drain", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float DrainInterpSpeed = 6.0f;

	/** 깎인 양을 읽을 수 있도록 하강을 시작하기 전에 잔상을 멈춰 두는 시간입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Delayed Drain", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float DrainHoldDuration = 0.2f;

	/** 지수 보간의 꼬리를 끊고 실제 체력에 맞추는 레이어 단위 간격입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Delayed Drain", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float DrainSnapThreshold = 0.002f;

private:
	/** MVVM Context Source로 해석된 보스 상태 ViewModel의 일회성 연출 이벤트를 구독합니다 */
	void BindToBossStatusViewModel();

	/** Widget 재구성이나 파괴 전에 기존 ViewModel 이벤트 구독을 해제합니다 */
	void UnbindFromBossStatusViewModel();

	/** 애니메이션이 도달한 레이어를 기준으로 실제 체력 바와 잔상 바의 색을 갱신합니다 */
	void ApplyLayerColors(int32 DisplayRemainingLayerCount);

	/** 0부터 3까지의 반복 순번을 실제 표시 색으로 변환합니다 */
	FLinearColor GetLayerColor(int32 ColorIndex) const;

	/** 현재 위젯 트리의 공통 루트에 Translation을 적용합니다 */
	void ApplyShakeTranslation(const FVector2D& Translation);

	/** 위젯 트리에서 메인 체력 ProgressBar를 찾습니다 */
	UProgressBar* FindMainHealthProgressBar() const;

	/** 위젯 트리에서 잔상 ProgressBar를 찾으며 아직 배치하지 않은 에셋에서는 nullptr를 반환합니다 */
	UProgressBar* FindDelayedHealthProgressBar() const;

	/** 체력이 줄어든 경우에만 잔상 하강을 다시 멈춰 두어 새 피해량을 읽을 시간을 만듭니다 */
	void HandleHealthLayerScaledChanged(UObject* Object, UE::FieldNotification::FFieldId FieldId);

	/** 실제 체력 바와 잔상 위치를 한 프레임만큼 진행하고 두 ProgressBar에 반영합니다 */
	void UpdateHealthBarAnimation(float InDeltaTime);

	UPROPERTY(Transient)
	TWeakObjectPtr<URSBossStatusViewModel> BoundBossStatusViewModel;

	FDelegateHandle HealthNormalizedChangedHandle;

	/** 0부터 레이어 수까지의 연속 좌표에서 실제 체력 바가 보여 주는 위치이며 게임플레이 체력이 아닙니다 */
	float DisplayLayerScaled = 0.0f;

	/** 같은 좌표에서 잔상이 머무는 위치이며 항상 실제 체력 바보다 뒤에 있습니다 */
	float DelayedLayerScaled = 0.0f;

	float DrainHoldRemaining = 0.0f;

	/** 매 프레임 Brush를 다시 만들지 않도록 마지막으로 색을 적용한 레이어 수를 기억합니다 */
	int32 AppliedRemainingLayerCount = INDEX_NONE;

	bool bIsShakeActive = false;
	float ShakeElapsed = 0.0f;
	float CurrentShakeStrength = 0.0f;
};
