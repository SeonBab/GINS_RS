#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "RSFireballChargeWidgetUtilities.h"

namespace
{
	const FLinearColor FireballTestFilledColor(0.85f, 0.08f, 0.08f, 1.0f);
	const FLinearColor FireballTestEmptyColor(1.0f, 1.0f, 1.0f, 1.0f);
	constexpr float FireballTestPipSpacing = 4.0f;

	/** 지정한 칸이 채워진 색인지 확인합니다 */
	bool IsPipFilled(const UHorizontalBox& PipBox, int32 PipIndex)
	{
		const UImage* Pip = Cast<UImage>(PipBox.GetChildAt(PipIndex));
		return Pip && Pip->GetColorAndOpacity().Equals(FireballTestFilledColor);
	}

	/** 칸이 만들어지지 않았을 때 테스트가 크래시 대신 실패하도록 Slot을 안전하게 얻습니다 */
	const UHorizontalBoxSlot* GetPipSlot(const UHorizontalBox& PipBox, int32 PipIndex)
	{
		const UWidget* Pip = PipBox.GetChildAt(PipIndex);
		return Pip ? Cast<UHorizontalBoxSlot>(Pip->Slot) : nullptr;
	}

	/** 테스트가 읽기 쉽도록 고정 색과 간격으로 체력 칸을 적용합니다 */
	void ApplyTestHitPointPips(UHorizontalBox& PipBox, int32 CurrentHitPoints, int32 MaxHitPoints)
	{
		RSFireballCharge::ApplyHitPointPips(PipBox, CurrentHitPoints, MaxHitPoints, FireballTestFilledColor, FireballTestEmptyColor, FireballTestPipSpacing);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSFireballChargeHitPointPipsTest, "RS.UI.FireballCharge.HitPointPips", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSFireballChargeHitPointPipCountTest, "RS.UI.FireballCharge.HitPointPipCount", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSFireballChargeHitPointPipsTest::RunTest(const FString& Parameters)
{
	UHorizontalBox* PipBox = NewObject<UHorizontalBox>();

	ApplyTestHitPointPips(*PipBox, 2, 2);
	TestEqual(TEXT("Full health shows one pip per hit point"), PipBox->GetChildrenCount(), 2);
	TestTrue(TEXT("Full health fills the left pip"), IsPipFilled(*PipBox, 0));
	TestTrue(TEXT("Full health fills the right pip"), IsPipFilled(*PipBox, 1));

	// 오른쪽 칸부터 비우기로 했으므로 한 대 맞으면 왼쪽 칸만 남습니다
	ApplyTestHitPointPips(*PipBox, 1, 2);
	TestTrue(TEXT("One hit keeps the left pip filled"), IsPipFilled(*PipBox, 0));
	TestFalse(TEXT("One hit empties the right pip"), IsPipFilled(*PipBox, 1));

	ApplyTestHitPointPips(*PipBox, 0, 2);
	TestFalse(TEXT("No health empties the left pip"), IsPipFilled(*PipBox, 0));
	TestFalse(TEXT("No health empties the right pip"), IsPipFilled(*PipBox, 1));

	ApplyTestHitPointPips(*PipBox, 5, 2);
	TestTrue(TEXT("Health above the maximum fills every pip"), IsPipFilled(*PipBox, 0) && IsPipFilled(*PipBox, 1));

	ApplyTestHitPointPips(*PipBox, -1, 2);
	TestFalse(TEXT("Health below zero empties every pip"), IsPipFilled(*PipBox, 0) || IsPipFilled(*PipBox, 1));

	return true;
}

bool FRSFireballChargeHitPointPipCountTest::RunTest(const FString& Parameters)
{
	UHorizontalBox* PipBox = NewObject<UHorizontalBox>();

	// 필요 타격 수는 기획자가 바꿀 수 있으므로 칸 수는 고정하지 않고 최대 체력을 따라갑니다
	ApplyTestHitPointPips(*PipBox, 3, 3);
	TestEqual(TEXT("A different hit count produces that many pips"), PipBox->GetChildrenCount(), 3);

	const UWidget* FirstPipBeforeSameCount = PipBox->GetChildAt(0);
	ApplyTestHitPointPips(*PipBox, 1, 3);
	TestTrue(TEXT("An unchanged pip count keeps the existing pips"), PipBox->GetChildAt(0) == FirstPipBeforeSameCount);

	ApplyTestHitPointPips(*PipBox, 2, 2);
	TestEqual(TEXT("A lowered hit count rebuilds the pips"), PipBox->GetChildrenCount(), 2);

	ApplyTestHitPointPips(*PipBox, 0, 0);
	TestEqual(TEXT("A hit count of zero leaves no pip"), PipBox->GetChildrenCount(), 0);

	ApplyTestHitPointPips(*PipBox, 0, -1);
	TestEqual(TEXT("A negative hit count leaves no pip"), PipBox->GetChildrenCount(), 0);

	ApplyTestHitPointPips(*PipBox, 2, 2);
	const UHorizontalBoxSlot* FirstPipSlot = GetPipSlot(*PipBox, 0);
	const UHorizontalBoxSlot* LastPipSlot = GetPipSlot(*PipBox, 1);
	if (!TestNotNull(TEXT("The first pip sits in a horizontal box slot"), FirstPipSlot) || !TestNotNull(TEXT("The last pip sits in a horizontal box slot"), LastPipSlot))
	{
		return false;
	}

	TestEqual(TEXT("The first pip keeps its outer edge tight to the border"), FirstPipSlot->GetPadding().Left, 0.0f);
	TestEqual(TEXT("The last pip keeps its outer edge tight to the border"), LastPipSlot->GetPadding().Right, 0.0f);
	TestTrue(TEXT("Neighbouring pips leave a gap that shows the divider"), FirstPipSlot->GetPadding().Right + LastPipSlot->GetPadding().Left > 0.0f);

	return true;
}

#endif
