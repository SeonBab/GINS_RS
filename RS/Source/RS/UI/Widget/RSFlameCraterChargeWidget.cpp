#include "RSFlameCraterChargeWidget.h"

#include "Components/ProgressBar.h"

void URSFlameCraterChargeWidget::SetChargeProgress(float InChargeProgress)
{
	if (ChargeProgressBar)
	{
		ChargeProgressBar->SetPercent(FMath::Clamp(InChargeProgress, 0.0f, 1.0f));
	}
}
