#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "RSAttackTelegraphComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSTelegraphLeadTimeTest, "RS.Combat.Telegraph.LeadTime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSTelegraphLeadTimeTest::RunTest(const FString& Parameters)
{
	// 표준 예고는 Fill 완료, 소멸, 판정이 이 순서로 벌어집니다
	FRSTelegraphLeadTime LeadTime;
	LeadTime.FillLeadTime = 0.15f;
	LeadTime.HideLeadTime = 0.05f;

	const float ImpactDelay = 1.0f;
	TestEqual(TEXT("Fill completes FillLeadTime before the hit"), LeadTime.GetFillDuration(ImpactDelay), 0.85f);
	TestEqual(TEXT("Telegraph hides HideLeadTime before the hit"), LeadTime.GetHideTime(ImpactDelay), 0.95f);
	TestTrue(TEXT("Fill completes before the telegraph hides"), LeadTime.GetFillDuration(ImpactDelay) < LeadTime.GetHideTime(ImpactDelay));
	TestTrue(TEXT("Telegraph hides before the hit"), LeadTime.GetHideTime(ImpactDelay) < ImpactDelay);

	const FRSTelegraphPresentation Presentation = LeadTime.MakePresentation(ImpactDelay);
	TestEqual(TEXT("Presentation fills for the lead-adjusted duration"), Presentation.FillDuration, 0.85f);
	TestEqual(TEXT("Presentation holds until the hide time"), Presentation.HoldDuration, 0.95f);

	TestTrue(TEXT("Standard lead times are valid"), LeadTime.IsDataValid(ImpactDelay));

	// ImpactDelay가 선행 시간보다 짧으면 Fill을 줄여서라도 순서를 유지합니다
	const float ShortImpactDelay = 0.1f;
	TestEqual(TEXT("Short impact delay clamps the fill duration to zero"), LeadTime.GetFillDuration(ShortImpactDelay), 0.0f);
	TestEqual(TEXT("Short impact delay still hides before the hit"), LeadTime.GetHideTime(ShortImpactDelay), 0.05f);
	TestFalse(TEXT("Short impact delay is rejected"), LeadTime.IsDataValid(ShortImpactDelay));

	// Fill이 소멸보다 늦게 끝나는 저술은 Fill을 앞당겨 끝까지 진행시킵니다
	FRSTelegraphLeadTime InvertedLeadTime;
	InvertedLeadTime.FillLeadTime = 0.02f;
	InvertedLeadTime.HideLeadTime = 0.5f;
	TestEqual(TEXT("Inverted lead times still hide at the hide time"), InvertedLeadTime.GetHideTime(ImpactDelay), 0.5f);
	TestEqual(TEXT("Inverted lead times pull the fill completion back to the hide time"), InvertedLeadTime.GetFillDuration(ImpactDelay), 0.5f);

	FString ValidationError;
	TestFalse(TEXT("Inverted lead times are rejected"), InvertedLeadTime.IsDataValid(ImpactDelay, &ValidationError));
	TestFalse(TEXT("Inverted lead times report why"), ValidationError.IsEmpty());

	// 선행 시간이 없으면 기존 동작과 같게 판정 시점에 채움이 끝나고 사라집니다
	FRSTelegraphLeadTime NoLeadTime;
	NoLeadTime.FillLeadTime = 0.0f;
	NoLeadTime.HideLeadTime = 0.0f;
	TestEqual(TEXT("Zero lead fills for the whole impact delay"), NoLeadTime.GetFillDuration(ImpactDelay), ImpactDelay);
	TestEqual(TEXT("Zero lead hides at the hit"), NoLeadTime.GetHideTime(ImpactDelay), ImpactDelay);

	return true;
}

#endif
