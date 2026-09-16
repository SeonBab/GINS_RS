#include "RSFireballChargeWidget.h"

#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "RSFireballChargeWidgetUtilities.h"

namespace RSFireballCharge
{
	void ApplyHitPointPips(UHorizontalBox& PipBox, int32 CurrentHitPoints, int32 MaxHitPoints, const FLinearColor& FilledColor, const FLinearColor& EmptyColor, float PipSpacing)
	{
		const int32 PipCount = FMath::Max(MaxHitPoints, 0);
		const int32 FilledPipCount = FMath::Clamp(CurrentHitPoints, 0, PipCount);

		// 칸 수는 필요 타격 수가 바뀔 때만 달라지므로 피격으로 색만 바뀌는 경우에는 칸을 다시 만들지 않습니다
		if (PipBox.GetChildrenCount() != PipCount)
		{
			PipBox.ClearChildren();

			for (int32 PipIndex = 0; PipIndex < PipCount; ++PipIndex)
			{
				UImage* Pip = NewObject<UImage>(&PipBox);
				UHorizontalBoxSlot* PipSlot = PipBox.AddChildToHorizontalBox(Pip);
				if (!PipSlot)
				{
					continue;
				}

				// 칸은 남은 폭을 균등하게 나눠 가지며 높이는 바깥 Box가 정합니다
				PipSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				PipSlot->SetHorizontalAlignment(HAlign_Fill);
				PipSlot->SetVerticalAlignment(VAlign_Fill);

				// 칸을 나누는 선은 이웃한 칸 사이에만 필요하고 바깥 테두리는 배경이 담당하므로 양 끝은 띄우지 않습니다
				const float LeftPadding = PipIndex > 0 ? PipSpacing * 0.5f : 0.0f;
				const float RightPadding = PipIndex < PipCount - 1 ? PipSpacing * 0.5f : 0.0f;
				PipSlot->SetPadding(FMargin(LeftPadding, 0.0f, RightPadding, 0.0f));
			}
		}

		for (int32 PipIndex = 0; PipIndex < PipCount; ++PipIndex)
		{
			// 오른쪽 칸부터 비우므로 앞선 칸이 남은 타격 수입니다
			if (UImage* Pip = Cast<UImage>(PipBox.GetChildAt(PipIndex)))
			{
				Pip->SetColorAndOpacity(PipIndex < FilledPipCount ? FilledColor : EmptyColor);
			}
		}
	}
}

void URSFireballChargeWidget::SetChargeProgress(float InChargeProgress)
{
	if (ChargeProgressBar)
	{
		ChargeProgressBar->SetPercent(FMath::Clamp(InChargeProgress, 0.0f, 1.0f));
	}
}

void URSFireballChargeWidget::SetHitPoints(int32 InCurrentHitPoints, int32 InMaxHitPoints)
{
	if (HitPointBox)
	{
		RSFireballCharge::ApplyHitPointPips(*HitPointBox, InCurrentHitPoints, InMaxHitPoints, FilledHitPointColor, EmptyHitPointColor, HitPointPipSpacing);
	}
}
