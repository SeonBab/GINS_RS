#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RSFlameCraterChargeWidget.generated.h"

class UProgressBar;

/** 화염 분화구의 착지 후 충전 진행도를 가로형 Progress Bar로 표시합니다 */
UCLASS(Abstract, Blueprintable)
class RS_API URSFlameCraterChargeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 충전 진행도를 0과 1 사이로 제한해 Progress Bar에 반영합니다 */
	void SetChargeProgress(float InChargeProgress);

protected:
	/** Blueprint가 같은 이름으로 배치하면 C++에서 직접 갱신할 Progress Bar입니다 */
	UPROPERTY(BlueprintReadOnly, Category = "RS|FlameCrater", meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> ChargeProgressBar;
};
