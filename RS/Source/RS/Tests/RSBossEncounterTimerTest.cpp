#if WITH_DEV_AUTOMATION_TESTS

// URSBossEncounterViewModel의 제한 시간과 Result 표시 규칙을 검증합니다
// 제한 시간 타이머의 예약과 정리, Encounter 생명주기 배선은 FTimerManager와 실제 월드가 필요하므로 PIE 검증이 담당합니다

#include "Misc/AutomationTest.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "RSBossEncounter.h"
#include "RSBossEncounterViewModel.h"
#include "RSLocalPlayerViewModelSubsystem.h"
#include "View/MVVMViewClass.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSBossEncounterTimerTest, "RS.UI.BossEncounterTimer", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSBossEncounterTimerTest::RunTest(const FString& Parameters)
{
	URSBossEncounterViewModel* ViewModel = NewObject<URSBossEncounterViewModel>();

	// 분과 초의 경계에서 자리수와 올림이 어긋나지 않아야 합니다
	const int32 FormatSeconds[] = { -1, 0, 1, 59, 60, 61, 599, 600, 3599, 3600 };
	const TCHAR* FormatTexts[] = { TEXT("00:00"), TEXT("00:00"), TEXT("00:01"), TEXT("00:59"), TEXT("01:00"), TEXT("01:01"), TEXT("09:59"), TEXT("10:00"), TEXT("59:59"), TEXT("60:00") };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(FormatSeconds); ++Index)
	{
		const FString Context = FString::Printf(TEXT("Format %d"), FormatSeconds[Index]);
		TestEqual(Context, URSBossEncounterViewModel::MakeRemainingTimeText(FormatSeconds[Index]).ToString(), FString(FormatTexts[Index]));
	}

	// 실제로 시간이 남아 있는 동안에는 00:00을 표시하지 않아야 합니다
	const float CeilingSeconds[] = { 0.0f, 0.001f, 0.99f, 1.0f, 1.01f, 298.5f, 299.001f };
	const TCHAR* CeilingTexts[] = { TEXT("00:00"), TEXT("00:01"), TEXT("00:01"), TEXT("00:01"), TEXT("00:02"), TEXT("04:59"), TEXT("05:00") };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(CeilingSeconds); ++Index)
	{
		ViewModel->ResetTimerValues();
		ViewModel->ApplyTimerValues(ERSBossEncounterState::Active, false, CeilingSeconds[Index]);
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
		{ TEXT("Preparing"), ERSBossEncounterState::Preparing, false, 0.0f, TEXT(""), false, false },
		{ TEXT("Active running"), ERSBossEncounterState::Active, false, 73.0f, TEXT("01:13"), true, true },
		{ TEXT("Active expired"), ERSBossEncounterState::Active, true, 0.0f, TEXT("00:00"), true, false },
		{ TEXT("Finished in time"), ERSBossEncounterState::Finished, false, 71.0f, TEXT("01:11"), true, false },
		{ TEXT("Finished after expiration"), ERSBossEncounterState::Finished, true, 0.0f, TEXT("00:00"), true, false }
	};

	for (const FDisplayCase& DisplayCase : DisplayCases)
	{
		ViewModel->ResetTimerValues();
		ViewModel->ApplyTimerValues(DisplayCase.EncounterState, DisplayCase.bHasExpired, DisplayCase.RemainingSeconds);
		const FString Context = FString(DisplayCase.Context);
		TestEqual(Context + TEXT(" text"), ViewModel->RemainingTimeText.ToString(), FString(DisplayCase.ExpectedText));
		TestEqual(Context + TEXT(" visible"), ViewModel->bIsVisible, DisplayCase.bExpectedVisible);
		TestEqual(Context + TEXT(" tickable"), ViewModel->IsTickable(), DisplayCase.bExpectedTickable);
	}

	// FText는 내용이 같아도 새로 만들면 다른 값이 되므로 같은 초에서는 다시 만들지 않아야 합니다
	ViewModel->ResetTimerValues();
	ViewModel->ApplyTimerValues(ERSBossEncounterState::Active, false, 120.4f);
	const FText FirstText = ViewModel->RemainingTimeText;
	ViewModel->ApplyTimerValues(ERSBossEncounterState::Active, false, 120.1f);
	TestTrue(TEXT("Same display second keeps text"), ViewModel->RemainingTimeText.IdenticalTo(FirstText));
	ViewModel->ApplyTimerValues(ERSBossEncounterState::Active, false, 119.9f);
	TestFalse(TEXT("New display second rebuilds text"), ViewModel->RemainingTimeText.IdenticalTo(FirstText));
	TestEqual(TEXT("New display second text"), ViewModel->RemainingTimeText.ToString(), FString(TEXT("02:00")));

	// 초기화 후 우연히 같은 초가 들어와도 이전 표시 이력 때문에 빈 문자열이 남으면 안 됩니다
	ViewModel->ApplyTimerValues(ERSBossEncounterState::Finished, false, 42.0f);
	TestEqual(TEXT("Finished snapshot text"), ViewModel->RemainingTimeText.ToString(), FString(TEXT("00:42")));
	ViewModel->ResetTimerValues();
	TestTrue(TEXT("Reset clears text"), ViewModel->RemainingTimeText.IsEmpty());
	ViewModel->ApplyTimerValues(ERSBossEncounterState::Active, false, 42.0f);
	TestEqual(TEXT("Same second after reset"), ViewModel->RemainingTimeText.ToString(), FString(TEXT("00:42")));

	const auto IsResultMessageCandidate = [](ERSBossEncounterResult Result, const FText& Message)
	{
		for (const FText& Candidate : URSBossEncounterViewModel::GetResultMessageCandidates(Result))
		{
			if (Candidate.EqualTo(Message))
			{
				return true;
			}
		}

		return false;
	};

	// Result Message는 해당 후보에서 Result Cycle당 한 번 선택하고 재동기화에서는 유지해야 합니다
	ViewModel->ResetResultValues();
	ViewModel->ApplyResultValues(ERSBossEncounterState::Finished, ERSBossEncounterResult::Clear);
	TestEqual(TEXT("Clear result value"), ViewModel->Result, ERSBossEncounterResult::Clear);
	TestEqual(TEXT("Clear result title"), ViewModel->ResultTitle.ToString(), FString(TEXT("CLEAR")));
	TestTrue(TEXT("Clear result message belongs to candidates"), IsResultMessageCandidate(ERSBossEncounterResult::Clear, ViewModel->ResultMessage));
	const FText FirstClearMessage = ViewModel->ResultMessage;
	ViewModel->ApplyResultValues(ERSBossEncounterState::Finished, ERSBossEncounterResult::Clear);
	TestTrue(TEXT("Same Result Cycle keeps selected message"), ViewModel->ResultMessage.IdenticalTo(FirstClearMessage));

	ViewModel->ApplyResultValues(ERSBossEncounterState::Inactive, ERSBossEncounterResult::None);
	TestEqual(TEXT("New Result Cycle clears result"), ViewModel->Result, ERSBossEncounterResult::None);
	TestTrue(TEXT("New Result Cycle clears title"), ViewModel->ResultTitle.IsEmpty());
	TestTrue(TEXT("New Result Cycle clears message"), ViewModel->ResultMessage.IsEmpty());
	TestFalse(TEXT("New Result Cycle clears selection history"), ViewModel->bHasSelectedResultMessage);

	ViewModel->ApplyResultValues(ERSBossEncounterState::Finished, ERSBossEncounterResult::Failed);
	TestEqual(TEXT("Failed result value"), ViewModel->Result, ERSBossEncounterResult::Failed);
	TestEqual(TEXT("Failed result title"), ViewModel->ResultTitle.ToString(), FString(TEXT("FAILED")));
	TestTrue(TEXT("Failed result message belongs to candidates"), IsResultMessageCandidate(ERSBossEncounterResult::Failed, ViewModel->ResultMessage));

	// 부모 Manual Source와 정적 자식 Context Source는 클래스와 이름이 모두 일치해야 합니다
	const auto FindEncounterSourceName = [this](const TCHAR* WidgetClassPath) -> FName
	{
		UClass* WidgetClass = LoadClass<UUserWidget>(nullptr, WidgetClassPath);
		UWidgetBlueprintGeneratedClass* GeneratedClass = Cast<UWidgetBlueprintGeneratedClass>(WidgetClass);
		const UMVVMViewClass* ViewClass = GeneratedClass ? GeneratedClass->GetExtension<UMVVMViewClass>() : nullptr;
		if (!TestNotNull(FString::Printf(TEXT("MVVM ViewClass %s"), WidgetClassPath), ViewClass))
		{
			return FName();
		}

		for (const FMVVMViewClass_Source& Source : ViewClass->GetSources())
		{
			if (Source.IsViewModel() && Source.GetSourceClass() == URSBossEncounterViewModel::StaticClass())
			{
				return Source.GetName();
			}
		}

		AddError(FString::Printf(TEXT("Boss Encounter ViewModel Source was not found: %s"), WidgetClassPath));
		return FName();
	};

	const FName ExpectedSourceName(TEXT("BossEncounterViewModel"));
	TestEqual(TEXT("Player HUD Manual Source name"), FindEncounterSourceName(TEXT("/Game/Blueprints/Widget/WBP_PlayerHUD.WBP_PlayerHUD_C")), ExpectedSourceName);
	TestEqual(TEXT("Boss Timer Context Source name"), FindEncounterSourceName(TEXT("/Game/Blueprints/Widget/WBP_BossTimer.WBP_BossTimer_C")), ExpectedSourceName);
	TestEqual(TEXT("Boss Result Context Source name"), FindEncounterSourceName(TEXT("/Game/Blueprints/Widget/WBP_BossResult.WBP_BossResult_C")), ExpectedSourceName);

	// ViewModel 저장소는 LocalPlayer 수명을 따르므로 World가 교체되어도 이전 전투의 표시 값이 남지 않아야 합니다
	UWorld* FirstWorld = UWorld::CreateWorld(EWorldType::Game, false);
	UWorld* SecondWorld = UWorld::CreateWorld(EWorldType::Game, false);
	ARSBossEncounter* FirstEncounter = FirstWorld->SpawnActor<ARSBossEncounter>();
	ARSBossEncounter* SecondEncounter = SecondWorld->SpawnActor<ARSBossEncounter>();
	// ULocalPlayerSubsystem의 ClassWithin 계층을 테스트에서도 그대로 구성합니다
	ULocalPlayer* TestLocalPlayer = NewObject<ULocalPlayer>(GEngine);
	URSLocalPlayerViewModelSubsystem* ViewModelSubsystem = NewObject<URSLocalPlayerViewModelSubsystem>(TestLocalPlayer);
	URSBossEncounterViewModel* SharedViewModel = ViewModelSubsystem->GetOrCreateViewModel<URSBossEncounterViewModel>();

	ViewModelSubsystem->RegisterSource(FirstEncounter);
	TestEqual(TEXT("First World Source is registered"), ViewModelSubsystem->Sources.Num(), 1);
	TestTrue(TEXT("Shared ViewModel observes first World Encounter"), SharedViewModel->BossEncounter.Get() == FirstEncounter);

	// 다른 World의 정리는 현재 원본을 해제하지 않아야 합니다
	ViewModelSubsystem->HandleWorldBeginTearDown(SecondWorld);
	TestEqual(TEXT("Other World tear down keeps Source"), ViewModelSubsystem->Sources.Num(), 1);
	TestTrue(TEXT("Other World tear down keeps observation"), SharedViewModel->BossEncounter.Get() == FirstEncounter);

	SharedViewModel->ApplyTimerValues(ERSBossEncounterState::Finished, false, 42.0f);
	SharedViewModel->ApplyResultValues(ERSBossEncounterState::Finished, ERSBossEncounterResult::Clear);
	ViewModelSubsystem->HandleWorldBeginTearDown(FirstWorld);
	TestEqual(TEXT("Torn down World Source is removed"), ViewModelSubsystem->Sources.Num(), 0);
	TestNull(TEXT("Torn down World clears observation"), SharedViewModel->BossEncounter.Get());
	TestFalse(TEXT("Torn down World hides Timer"), SharedViewModel->bIsVisible);
	TestTrue(TEXT("Torn down World clears Result Title"), SharedViewModel->ResultTitle.IsEmpty());
	TestTrue(TEXT("Torn down World clears Result Message"), SharedViewModel->ResultMessage.IsEmpty());
	TestEqual(TEXT("Torn down World clears Result"), SharedViewModel->Result, ERSBossEncounterResult::None);

	// 이전 World의 해제를 놓친 경우에도 새 World의 원본이 들어오면 남은 원본을 정리해야 합니다
	ViewModelSubsystem->RegisterSource(SecondEncounter);
	ViewModelSubsystem->RegisterSource(FirstEncounter);
	TestEqual(TEXT("Cross World registration keeps one Source"), ViewModelSubsystem->Sources.Num(), 1);
	TestTrue(TEXT("Cross World registration replaces observation"), SharedViewModel->BossEncounter.Get() == FirstEncounter);

	SharedViewModel->UninitializeViewModel();
	FirstWorld->DestroyWorld(false);
	SecondWorld->DestroyWorld(false);

	// Tickable 객체가 갱신이 필요한 상태로 남지 않도록 마무리합니다
	ViewModel->UninitializeViewModel();
	TestFalse(TEXT("Uninitialized is not tickable"), ViewModel->IsTickable());

	return true;
}

#endif
