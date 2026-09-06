#if WITH_DEV_AUTOMATION_TESTS

// URSBossEncounterTimerViewModel의 표시 규칙만 검증합니다
// 제한 시간 타이머의 예약과 정리, Encounter 생명주기 배선은 FTimerManager와 실제 월드가 필요하므로 PIE 검증이 담당합니다

#include "Misc/AutomationTest.h"
#include "RSBossEncounter.h"
#include "RSBossEncounterTimerViewModel.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSBossEncounterTimerTest, "RS.UI.BossEncounterTimer", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSBossEncounterTimerTest::RunTest(const FString& Parameters)
{
	URSBossEncounterTimerViewModel* ViewModel = NewObject<URSBossEncounterTimerViewModel>();

	// 분과 초의 경계에서 자리수와 올림이 어긋나지 않아야 합니다
	const int32 FormatSeconds[] = { -1, 0, 1, 59, 60, 61, 599, 600, 3599, 3600 };
	const TCHAR* FormatTexts[] = { TEXT("00:00"), TEXT("00:00"), TEXT("00:01"), TEXT("00:59"), TEXT("01:00"), TEXT("01:01"), TEXT("09:59"), TEXT("10:00"), TEXT("59:59"), TEXT("60:00") };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(FormatSeconds); ++Index)
	{
		const FString Context = FString::Printf(TEXT("Format %d"), FormatSeconds[Index]);
		TestEqual(Context, URSBossEncounterTimerViewModel::MakeRemainingTimeText(FormatSeconds[Index]).ToString(), FString(FormatTexts[Index]));
	}

	// 실제로 시간이 남아 있는 동안에는 00:00을 표시하지 않아야 합니다
	const float CeilingSeconds[] = { 0.0f, 0.001f, 0.99f, 1.0f, 1.01f, 298.5f, 299.001f };
	const TCHAR* CeilingTexts[] = { TEXT("00:00"), TEXT("00:01"), TEXT("00:01"), TEXT("00:01"), TEXT("00:02"), TEXT("04:59"), TEXT("05:00") };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(CeilingSeconds); ++Index)
	{
		ViewModel->ResetTimerValues();
		ViewModel->ApplyEncounterValues(ERSBossEncounterState::Active, false, CeilingSeconds[Index]);
		const FString Context = FString::Printf(TEXT("Ceiling %.3f"), CeilingSeconds[Index]);
		TestEqual(Context, ViewModel->RemainingTimeText.ToString(), FString(CeilingTexts[Index]));
	}

	// 설계 문서의 표시 규칙과 Tick 활성 조건을 상태별로 고정합니다
	struct FDisplayCase
	{
		const TCHAR* Context;
		ERSBossEncounterState EncounterState;
		bool bHasExpired;
		float RemainingSeconds;
		const TCHAR* ExpectedText;
		bool bExpectedVisible;
		bool bExpectedTickable;
	};

	const FDisplayCase DisplayCases[] =
	{
		{ TEXT("Inactive"), ERSBossEncounterState::Inactive, false, 0.0f, TEXT(""), false, false },
		{ TEXT("Active running"), ERSBossEncounterState::Active, false, 73.0f, TEXT("01:13"), true, true },
		{ TEXT("Active expired"), ERSBossEncounterState::Active, true, 0.0f, TEXT("00:00"), true, false },
		{ TEXT("Completed in time"), ERSBossEncounterState::Completed, false, 71.0f, TEXT("01:11"), true, false },
		{ TEXT("Completed after expiration"), ERSBossEncounterState::Completed, true, 0.0f, TEXT("00:00"), true, false }
	};

	for (const FDisplayCase& DisplayCase : DisplayCases)
	{
		ViewModel->ResetTimerValues();
		ViewModel->ApplyEncounterValues(DisplayCase.EncounterState, DisplayCase.bHasExpired, DisplayCase.RemainingSeconds);
		const FString Context = FString(DisplayCase.Context);
		TestEqual(Context + TEXT(" text"), ViewModel->RemainingTimeText.ToString(), FString(DisplayCase.ExpectedText));
		TestEqual(Context + TEXT(" visible"), ViewModel->bIsVisible, DisplayCase.bExpectedVisible);
		TestEqual(Context + TEXT(" tickable"), ViewModel->IsTickable(), DisplayCase.bExpectedTickable);
	}

	// FText는 내용이 같아도 새로 만들면 다른 값이 되므로 같은 초에서는 다시 만들지 않아야 합니다
	ViewModel->ResetTimerValues();
	ViewModel->ApplyEncounterValues(ERSBossEncounterState::Active, false, 120.4f);
	const FText FirstText = ViewModel->RemainingTimeText;
	ViewModel->ApplyEncounterValues(ERSBossEncounterState::Active, false, 120.1f);
	TestTrue(TEXT("Same display second keeps text"), ViewModel->RemainingTimeText.IdenticalTo(FirstText));
	ViewModel->ApplyEncounterValues(ERSBossEncounterState::Active, false, 119.9f);
	TestFalse(TEXT("New display second rebuilds text"), ViewModel->RemainingTimeText.IdenticalTo(FirstText));
	TestEqual(TEXT("New display second text"), ViewModel->RemainingTimeText.ToString(), FString(TEXT("02:00")));

	// 초기화 후 우연히 같은 초가 들어와도 이전 표시 이력 때문에 빈 문자열이 남으면 안 됩니다
	ViewModel->ApplyEncounterValues(ERSBossEncounterState::Completed, false, 42.0f);
	TestEqual(TEXT("Completed snapshot text"), ViewModel->RemainingTimeText.ToString(), FString(TEXT("00:42")));
	ViewModel->ResetTimerValues();
	TestTrue(TEXT("Reset clears text"), ViewModel->RemainingTimeText.IsEmpty());
	ViewModel->ApplyEncounterValues(ERSBossEncounterState::Active, false, 42.0f);
	TestEqual(TEXT("Same second after reset"), ViewModel->RemainingTimeText.ToString(), FString(TEXT("00:42")));

	// Tickable 객체가 갱신이 필요한 상태로 남지 않도록 마무리합니다
	ViewModel->UninitializeViewModel();
	TestFalse(TEXT("Uninitialized is not tickable"), ViewModel->IsTickable());

	return true;
}

#endif
