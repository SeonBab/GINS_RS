#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "RSBossPhaseComponent.h"
#include "RSGameplayAbility_BasicAttack.h"
#include "RSGameplayAbility_ConcentricRings.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSBossPhaseCycleTest, "RS.Boss.PhaseCycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSBossPhaseCycleTest::RunTest(const FString& Parameters)
{
	// 진행 컴포넌트는 Behavior Tree와 Blackboard를 참조하지 않으므로 월드 없이 사이클 계약을 검증할 수 있습니다
	URSBossPhaseComponent* PhaseComponent = NewObject<URSBossPhaseComponent>();

	const TArray<ERSBossPatternType> CycleSequence = { ERSBossPatternType::Basic, ERSBossPatternType::Basic, ERSBossPatternType::Special };
	const TArray<TSubclassOf<URSBaseGameplayAbility>> BasicPatterns = { URSGameplayAbility_BasicAttack::StaticClass() };
	const TArray<TSubclassOf<URSBaseGameplayAbility>> SpecialPatterns = { URSGameplayAbility_ConcentricRings::StaticClass() };

	PhaseComponent->SetPatternCycleForTest(CycleSequence, BasicPatterns, SpecialPatterns);

	ERSBossPatternType CurrentPatternType = ERSBossPatternType::Special;

	TestTrue(TEXT("Initial turn resolves"), PhaseComponent->TryGetCurrentPatternType(CurrentPatternType));
	TestEqual(TEXT("Cycle step 0 is basic"), CurrentPatternType, ERSBossPatternType::Basic);
	TestEqual(TEXT("Initial cycle index"), PhaseComponent->GetCurrentCycleIndex(), 0);

	// 진행을 호출하지 않으면 같은 차례가 유지됩니다
	TestTrue(TEXT("Turn is stable without advancing"), PhaseComponent->TryGetCurrentPatternType(CurrentPatternType));
	TestEqual(TEXT("Cycle step 0 stays basic"), CurrentPatternType, ERSBossPatternType::Basic);

	PhaseComponent->AdvancePatternCycle();
	TestTrue(TEXT("Second turn resolves"), PhaseComponent->TryGetCurrentPatternType(CurrentPatternType));
	TestEqual(TEXT("Cycle step 1 is basic"), CurrentPatternType, ERSBossPatternType::Basic);

	PhaseComponent->AdvancePatternCycle();
	TestTrue(TEXT("Third turn resolves"), PhaseComponent->TryGetCurrentPatternType(CurrentPatternType));
	TestEqual(TEXT("Cycle step 2 is special"), CurrentPatternType, ERSBossPatternType::Special);

	PhaseComponent->AdvancePatternCycle();
	TestEqual(TEXT("Cycle wraps to start"), PhaseComponent->GetCurrentCycleIndex(), 0);
	TestTrue(TEXT("Wrapped turn resolves"), PhaseComponent->TryGetCurrentPatternType(CurrentPatternType));
	TestEqual(TEXT("Wrapped step is basic"), CurrentPatternType, ERSBossPatternType::Basic);

	// 후보가 하나뿐이면 종류별로 항상 그 어빌리티가 선택됩니다
	TSubclassOf<URSBaseGameplayAbility> SelectedAbilityClass;
	TestTrue(TEXT("Basic selection succeeds"), PhaseComponent->TrySelectPattern(ERSBossPatternType::Basic, SelectedAbilityClass));
	TestEqual(TEXT("Basic selection uses basic list"), SelectedAbilityClass.Get(), URSGameplayAbility_BasicAttack::StaticClass());

	TestTrue(TEXT("Special selection succeeds"), PhaseComponent->TrySelectPattern(ERSBossPatternType::Special, SelectedAbilityClass));
	TestEqual(TEXT("Special selection uses special list"), SelectedAbilityClass.Get(), URSGameplayAbility_ConcentricRings::StaticClass());

	PhaseComponent->AdvancePatternCycle();
	PhaseComponent->ResetPatternCycle();
	TestEqual(TEXT("Reset returns to start"), PhaseComponent->GetCurrentCycleIndex(), 0);

	// 후보 목록이 비어 있으면 설정 오류이므로 선택이 실패합니다
	AddExpectedMessagePlain(TEXT("패턴 후보 목록이 비어 있습니다"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	PhaseComponent->SetPatternCycleForTest(CycleSequence, TArray<TSubclassOf<URSBaseGameplayAbility>>(), SpecialPatterns);
	TestFalse(TEXT("Empty candidate list fails"), PhaseComponent->TrySelectPattern(ERSBossPatternType::Basic, SelectedAbilityClass));

	// 사이클이 비어 있으면 어떤 차례도 성립하지 않습니다
	AddExpectedMessagePlain(TEXT("CycleSequence가 비어 있어"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	PhaseComponent->SetPatternCycleForTest(TArray<ERSBossPatternType>(), BasicPatterns, SpecialPatterns);
	TestFalse(TEXT("Empty cycle sequence fails"), PhaseComponent->TryGetCurrentPatternType(CurrentPatternType));

	// 사이클이 비어 있어도 진행 호출이 크래시하지 않습니다
	PhaseComponent->AdvancePatternCycle();
	TestEqual(TEXT("Empty cycle keeps index"), PhaseComponent->GetCurrentCycleIndex(), 0);

	return true;
}

#endif
