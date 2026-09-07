// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FieldNotificationId.h"
#include "RSBossHealthBarWidget.generated.h"

class URSBossStatusViewModel;

/**
 * 보스 체력바 전체에 짧은 절차적 피격 흔들림을 적용하는 위젯입니다
 * 지속 체력 상태는 MVVM FieldNotify가 담당하고 이 클래스는 일회성 연출만 처리합니다
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

private:
	/** MVVM Context Source로 해석된 보스 상태 ViewModel의 일회성 연출 이벤트를 구독합니다 */
	void BindToBossStatusViewModel();

	/** Widget 재구성이나 파괴 전에 기존 ViewModel 이벤트 구독을 해제합니다 */
	void UnbindFromBossStatusViewModel();

	/** 레이어 순번이나 남은 수가 바뀌면 하나의 ProgressBar 앞면과 뒷면 색을 함께 갱신합니다 */
	void HandleLayerVisualChanged(UObject* Object, UE::FieldNotification::FFieldId FieldId);

	/** 현재 ViewModel 값으로 메인 ProgressBar의 Fill과 Background Brush 색을 갱신합니다 */
	void ApplyLayerColors();

	/** 0부터 3까지의 반복 순번을 실제 표시 색으로 변환합니다 */
	FLinearColor GetLayerColor(int32 ColorIndex) const;

	/** 현재 위젯 트리의 공통 루트에 Translation을 적용합니다 */
	void ApplyShakeTranslation(const FVector2D& Translation);

	UPROPERTY(Transient)
	TWeakObjectPtr<URSBossStatusViewModel> BoundBossStatusViewModel;

	FDelegateHandle LayerColorChangedHandle;
	FDelegateHandle RemainingLayerCountChangedHandle;

	bool bIsShakeActive = false;
	float ShakeElapsed = 0.0f;
	float CurrentShakeStrength = 0.0f;
};
