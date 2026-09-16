#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RSFireballChargeWidget.generated.h"

class UHorizontalBox;
class UProgressBar;

/** 화염구의 착지 후 충전 진행도와 남은 타격 수를 표시합니다 */
UCLASS(Abstract, Blueprintable)
class RS_API URSFireballChargeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 충전 진행도를 0과 1 사이로 제한해 Progress Bar에 반영합니다 */
	void SetChargeProgress(float InChargeProgress);

	/**
	 * 남은 타격 수와 필요 타격 수를 체력 칸에 반영합니다
	 * 필요 타격 수는 기획자가 바꿀 수 있으므로 칸을 미리 배치하지 않고 이 값에 맞춰 만듭니다
	 */
	void SetHitPoints(int32 InCurrentHitPoints, int32 InMaxHitPoints);

protected:
	/** Blueprint가 같은 이름으로 배치하면 C++에서 직접 갱신할 Progress Bar입니다 */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Fireball", meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> ChargeProgressBar;

	/** Blueprint가 같은 이름으로 배치하면 C++이 체력 칸을 채워 넣을 가로 Box입니다 */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Fireball", meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> HitPointBox;

private:
	/** 아직 남아 있는 타격 한 번을 나타내는 칸의 색입니다 */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Fireball", meta = (AllowPrivateAccess = "true"))
	FLinearColor FilledHitPointColor = FLinearColor(0.85f, 0.08f, 0.08f, 1.0f);

	/** 이미 맞아서 비워진 칸의 색입니다 */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Fireball", meta = (AllowPrivateAccess = "true"))
	FLinearColor EmptyHitPointColor = FLinearColor::White;

	/** 칸 사이에 두는 간격이며 이만큼 뒤의 배경이 보여 칸을 나누는 선이 됩니다 */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Fireball", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float HitPointPipSpacing = 4.0f;
};
