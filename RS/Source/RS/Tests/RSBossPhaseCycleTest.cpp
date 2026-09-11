#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "RSBossPhaseComponent.h"
#include "RSBossPhaseData.h"
#include "RSGameplayAbility_ConcentricRings.h"
#include "RSGameplayAbility_TargetedSlam.h"

// 보고자는 추상 클래스라 인스턴스를 만들 수 없으므로 CDO를 사용합니다
#include "RSBaseGameplayAbility_BossPattern.h"

namespace
{
	/** 상시·특수 후보와 사이클 순서를 갖춘 페이즈 정의를 만듭니다 */
	FRSBossPhaseDefinition MakeTestPhase(TSubclassOf<URSBaseGameplayAbility> BasicPattern, TSubclassOf<URSBaseGameplayAbility> SpecialPattern, TSubclassOf<URSBaseGameplayAbility> MainGimmick, float HealthTriggerRatio)
	{
		FRSBossPhaseDefinition Phase;
		Phase.BasicPatterns = { BasicPattern };
		Phase.SpecialPatterns = { SpecialPattern };
		Phase.CycleSequence = { ERSBossPatternType::Basic, ERSBossPatternType::Basic, ERSBossPatternType::Special };
		Phase.MainGimmickAbility = MainGimmick;
		Phase.HealthTriggerRatio = HealthTriggerRatio;

		return Phase;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSBossPhaseCycleTest, "RS.Boss.PhaseCycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSBossPhaseCycleTest::RunTest(const FString& Parameters)
{
	// 진행 컴포넌트는 Behavior Tree와 Blackboard를 참조하지 않으므로 월드 없이 사이클 계약을 검증할 수 있습니다
	URSBossPhaseComponent* PhaseComponent = NewObject<URSBossPhaseComponent>();

	const TSubclassOf<URSBaseGameplayAbility> BasicPattern = URSGameplayAbility_TargetedSlam::StaticClass();
	const TSubclassOf<URSBaseGameplayAbility> SpecialPattern = URSGameplayAbility_ConcentricRings::StaticClass();

	TArray<FRSBossPhaseDefinition> Phases;
	Phases.Add(MakeTestPhase(BasicPattern, SpecialPattern, URSGameplayAbility_ConcentricRings::StaticClass(), 0.66f));
	Phases.Add(MakeTestPhase(BasicPattern, SpecialPattern, nullptr, 0.0f));

	PhaseComponent->SetPhasesForTest(Phases);
	TestTrue(TEXT("Empty initial phase entry is a successful no-op"), PhaseComponent->EnterCurrentPhase());

	ERSBossPatternType CurrentPatternType = ERSBossPatternType::Special;

	TestEqual(TEXT("Phase count"), PhaseComponent->GetPhaseCount(), 2);
	TestEqual(TEXT("Initial phase index"), PhaseComponent->GetCurrentPhaseIndex(), 0);
	TestEqual(TEXT("Initial cycle index"), PhaseComponent->GetCurrentCycleIndex(), 0);
	TestEqual(TEXT("Cycle length"), PhaseComponent->GetCycleLength(), 3);

	TestTrue(TEXT("Initial turn resolves"), PhaseComponent->TryGetCurrentPatternType(CurrentPatternType));
	TestEqual(TEXT("Cycle step 0 is basic"), CurrentPatternType, ERSBossPatternType::Basic);

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

	// 후보가 하나뿐이면 종류별로 항상 그 어빌리티가 선택됩니다
	TSubclassOf<URSBaseGameplayAbility> SelectedAbilityClass;
	TestTrue(TEXT("Basic selection succeeds"), PhaseComponent->TrySelectPattern(ERSBossPatternType::Basic, SelectedAbilityClass));
	TestEqual(TEXT("Basic selection uses basic list"), SelectedAbilityClass.Get(), BasicPattern.Get());

	TestTrue(TEXT("Special selection succeeds"), PhaseComponent->TrySelectPattern(ERSBossPatternType::Special, SelectedAbilityClass));
	TestEqual(TEXT("Special selection uses special list"), SelectedAbilityClass.Get(), SpecialPattern.Get());

	// 지속 오브젝트의 수명은 Ability 성공 콜백이 아니라 활성화 시도 직전 통지로 진행합니다
	URSBossPhaseComponent* LifetimePhaseComponent = NewObject<URSBossPhaseComponent>();
	LifetimePhaseComponent->SetPhasesForTest(Phases);

	int32 SpecialPatternRequestCount = 0;
	int32 LastSpecialPatternSequence = 0;
	int32 PersistentCleanupRequestCount = 0;
	LifetimePhaseComponent->OnSpecialPatternActivationRequested().AddLambda([&SpecialPatternRequestCount, &LastSpecialPatternSequence](int32 Sequence)
	{
		++SpecialPatternRequestCount;
		LastSpecialPatternSequence = Sequence;
	});
	LifetimePhaseComponent->OnPersistentObjectCleanupRequested().AddLambda([&PersistentCleanupRequestCount]()
	{
		++PersistentCleanupRequestCount;
	});

	LifetimePhaseComponent->NotifyAbilityActivationRequested(BasicPattern);
	TestEqual(TEXT("Basic pattern request does not advance persistent lifetime"), SpecialPatternRequestCount, 0);
	TestEqual(TEXT("Basic pattern request does not clean persistent objects"), PersistentCleanupRequestCount, 0);

	LifetimePhaseComponent->NotifyAbilityActivationRequested(SpecialPattern);
	LifetimePhaseComponent->NotifyAbilityActivationRequested(SpecialPattern);
	TestEqual(TEXT("Each special pattern request is broadcast before activation"), SpecialPatternRequestCount, 2);
	TestEqual(TEXT("Special pattern request sequence is not rolled back"), LastSpecialPatternSequence, 2);
	TestEqual(TEXT("Phase component keeps the requested special sequence"), LifetimePhaseComponent->GetSpecialPatternActivationSequence(), 2);

	// 같은 Class가 특수 후보와 메인 기믹에 모두 있어도 전환 대기에서는 메인 정리를 우선합니다
	LifetimePhaseComponent->EvaluateHealthTriggerForTest(40.0f, 100.0f);
	LifetimePhaseComponent->NotifyAbilityActivationRequested(SpecialPattern);
	TestEqual(TEXT("Main gimmick request immediately cleans persistent objects"), PersistentCleanupRequestCount, 1);
	TestEqual(TEXT("Main gimmick request does not advance the special sequence"), LifetimePhaseComponent->GetSpecialPatternActivationSequence(), 2);

	LifetimePhaseComponent->NotifyAbilityActivationRequested(nullptr);
	TestEqual(TEXT("Missing optional ability sends no lifetime event"), PersistentCleanupRequestCount, 1);

	PhaseComponent->AdvancePatternCycle();
	PhaseComponent->ResetPatternCycle();
	TestEqual(TEXT("Reset returns to start"), PhaseComponent->GetCurrentCycleIndex(), 0);

	// 기준 위에서는 전환 대기가 발행되지 않습니다
	PhaseComponent->EvaluateHealthTriggerForTest(80.0f, 100.0f);
	TestFalse(TEXT("Above trigger keeps transition idle"), PhaseComponent->IsPhaseTransitionPending());

	// 기준 지점을 한 번에 지나쳐도 누락되지 않습니다
	PhaseComponent->EvaluateHealthTriggerForTest(40.0f, 100.0f);
	TestTrue(TEXT("Crossing trigger requests transition"), PhaseComponent->IsPhaseTransitionPending());

	TSubclassOf<URSBaseGameplayAbility> PendingGimmick;
	TestTrue(TEXT("Pending gimmick is readable"), PhaseComponent->TryGetPendingMainGimmick(PendingGimmick));
	TestEqual(TEXT("Pending gimmick matches phase data"), PendingGimmick.Get(), URSGameplayAbility_ConcentricRings::StaticClass());

	// 페이즈 전환은 사이클과 전환 대기를 모두 초기화합니다
	PhaseComponent->AdvancePatternCycle();
	PhaseComponent->AdvanceToNextPhase();
	TestEqual(TEXT("Phase advanced"), PhaseComponent->GetCurrentPhaseIndex(), 1);
	TestTrue(TEXT("Empty later phase entry remains a successful no-op"), PhaseComponent->EnterCurrentPhase());
	TestEqual(TEXT("Phase change resets cycle"), PhaseComponent->GetCurrentCycleIndex(), 0);
	TestFalse(TEXT("Phase change clears pending transition"), PhaseComponent->IsPhaseTransitionPending());

	// 배열의 마지막 페이즈는 체력이 아무리 낮아도 전환을 발행하지 않습니다
	PhaseComponent->EvaluateHealthTriggerForTest(1.0f, 100.0f);
	TestFalse(TEXT("Last phase never requests transition"), PhaseComponent->IsPhaseTransitionPending());

	// 무력화는 기믹을 파훼했을 때만 재생합니다
	TArray<FRSBossPhaseDefinition> GroggyPhases;
	GroggyPhases.Add(MakeTestPhase(BasicPattern, SpecialPattern, URSGameplayAbility_ConcentricRings::StaticClass(), 0.5f));
	GroggyPhases.Add(MakeTestPhase(BasicPattern, SpecialPattern, nullptr, 0.0f));

	PhaseComponent->SetPhasesForTest(GroggyPhases);
	PhaseComponent->SetGroggyAbilityForTest(URSGameplayAbility_TargetedSlam::StaticClass());
	PhaseComponent->EvaluateHealthTriggerForTest(40.0f, 100.0f);

	// 모든 보스 패턴이 보고하므로 이번 페이즈의 기믹이 낸 보고만 수락하는지 함께 확인합니다
	const UGameplayAbility* GimmickReporter = GetDefault<URSGameplayAbility_ConcentricRings>();
	const UGameplayAbility* OtherPatternReporter = GetDefault<URSGameplayAbility_TargetedSlam>();

	TSubclassOf<URSBaseGameplayAbility> PendingGroggy;
	TestEqual(TEXT("Outcome starts as none"), PhaseComponent->GetMainGimmickOutcome(), ERSBossMainGimmickOutcome::None);
	TestFalse(TEXT("No groggy before judgement"), PhaseComponent->TryGetPendingGroggy(PendingGroggy));

	// 기믹이 아닌 패턴의 보고는 판정을 차지하지 않습니다
	PhaseComponent->ReportMainGimmickOutcome(OtherPatternReporter, ERSBossMainGimmickOutcome::Broken);
	TestEqual(TEXT("Non-gimmick pattern report is ignored"), PhaseComponent->GetMainGimmickOutcome(), ERSBossMainGimmickOutcome::None);

	PhaseComponent->ReportMainGimmickOutcome(GimmickReporter, ERSBossMainGimmickOutcome::NotBroken);
	TestEqual(TEXT("Gimmick report is accepted"), PhaseComponent->GetMainGimmickOutcome(), ERSBossMainGimmickOutcome::NotBroken);
	TestFalse(TEXT("No groggy when gimmick was not broken"), PhaseComponent->TryGetPendingGroggy(PendingGroggy));

	// 한 페이즈의 판정은 한 번만 정해지므로 뒤따르는 보고가 덮어쓰지 않습니다
	PhaseComponent->ReportMainGimmickOutcome(GimmickReporter, ERSBossMainGimmickOutcome::Broken);
	TestEqual(TEXT("Second report does not overwrite"), PhaseComponent->GetMainGimmickOutcome(), ERSBossMainGimmickOutcome::NotBroken);
	TestFalse(TEXT("Overwrite attempt grants no groggy"), PhaseComponent->TryGetPendingGroggy(PendingGroggy));

	// 파훼를 보고한 페이즈에서는 무력화가 재생됩니다
	// SetPhasesForTest가 PhaseData를 새로 만들므로 공용 무력화도 다시 지정합니다
	PhaseComponent->SetPhasesForTest(GroggyPhases);
	PhaseComponent->SetGroggyAbilityForTest(URSGameplayAbility_TargetedSlam::StaticClass());
	PhaseComponent->EvaluateHealthTriggerForTest(40.0f, 100.0f);
	PhaseComponent->ReportMainGimmickOutcome(GimmickReporter, ERSBossMainGimmickOutcome::Broken);
	TestTrue(TEXT("Groggy plays when gimmick was broken"), PhaseComponent->TryGetPendingGroggy(PendingGroggy));
	TestEqual(TEXT("Groggy uses the shared ability"), PendingGroggy.Get(), URSGameplayAbility_TargetedSlam::StaticClass());

	// 페이즈 전환은 판정 결과도 초기화해 다음 기믹에 영향을 주지 않습니다
	PhaseComponent->AdvanceToNextPhase();
	TestEqual(TEXT("Phase change clears outcome"), PhaseComponent->GetMainGimmickOutcome(), ERSBossMainGimmickOutcome::None);
	TestFalse(TEXT("Phase change clears groggy"), PhaseComponent->TryGetPendingGroggy(PendingGroggy));

	// 무력화 어빌리티가 설정되지 않은 보스는 파훼해도 무력화가 없습니다
	PhaseComponent->SetPhasesForTest(GroggyPhases);
	PhaseComponent->SetGroggyAbilityForTest(nullptr);
	PhaseComponent->EvaluateHealthTriggerForTest(40.0f, 100.0f);
	PhaseComponent->ReportMainGimmickOutcome(GimmickReporter, ERSBossMainGimmickOutcome::Broken);
	TestFalse(TEXT("No groggy without a configured ability"), PhaseComponent->TryGetPendingGroggy(PendingGroggy));

	// 전환 대기 중이 아니면 기믹 차례가 아니므로 일반 사이클의 보고로 보고 버립니다
	PhaseComponent->SetPhasesForTest(GroggyPhases);
	PhaseComponent->ReportMainGimmickOutcome(GimmickReporter, ERSBossMainGimmickOutcome::Broken);
	TestEqual(TEXT("Report outside a pending transition is ignored"), PhaseComponent->GetMainGimmickOutcome(), ERSBossMainGimmickOutcome::None);

	// 메인 기믹이 없어도 체력 기준만으로 다음 페이즈로 넘어갈 수 있습니다
	TArray<FRSBossPhaseDefinition> GimmicklessPhases;
	GimmicklessPhases.Add(MakeTestPhase(BasicPattern, SpecialPattern, nullptr, 0.5f));
	GimmicklessPhases.Add(MakeTestPhase(BasicPattern, SpecialPattern, nullptr, 0.0f));

	PhaseComponent->SetPhasesForTest(GimmicklessPhases);
	PhaseComponent->EvaluateHealthTriggerForTest(40.0f, 100.0f);
	TestTrue(TEXT("Gimmickless transition is requested"), PhaseComponent->IsPhaseTransitionPending());
	TestFalse(TEXT("Gimmickless transition has no gimmick"), PhaseComponent->TryGetPendingMainGimmick(PendingGimmick));

	PhaseComponent->AdvanceToNextPhase();
	TestEqual(TEXT("Gimmickless phase advanced"), PhaseComponent->GetCurrentPhaseIndex(), 1);
	TestFalse(TEXT("Gimmickless advance clears pending"), PhaseComponent->IsPhaseTransitionPending());

	// 마지막 페이즈에서 페이즈 전환을 요청해도 인덱스가 넘어가지 않습니다
	AddExpectedMessagePlain(TEXT("다음이 없어 페이즈 전환을 건너뜁니다"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	PhaseComponent->AdvanceToNextPhase();
	TestEqual(TEXT("Last phase stays"), PhaseComponent->GetCurrentPhaseIndex(), 1);

	// 후보 목록이 비어 있으면 설정 오류이므로 선택이 실패합니다
	TArray<FRSBossPhaseDefinition> EmptyCandidatePhases;
	EmptyCandidatePhases.Add(MakeTestPhase(nullptr, SpecialPattern, nullptr, 0.0f));
	EmptyCandidatePhases[0].BasicPatterns.Reset();

	AddExpectedMessagePlain(TEXT("패턴 후보가 없습니다"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	PhaseComponent->SetPhasesForTest(EmptyCandidatePhases);
	TestFalse(TEXT("Empty candidate list fails"), PhaseComponent->TrySelectPattern(ERSBossPatternType::Basic, SelectedAbilityClass));

	// 사이클이 비어 있으면 어떤 차례도 성립하지 않습니다
	TArray<FRSBossPhaseDefinition> EmptyCyclePhases;
	EmptyCyclePhases.Add(MakeTestPhase(BasicPattern, SpecialPattern, nullptr, 0.0f));
	EmptyCyclePhases[0].CycleSequence.Reset();

	AddExpectedMessagePlain(TEXT("CycleSequence가 없어"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	PhaseComponent->SetPhasesForTest(EmptyCyclePhases);
	TestFalse(TEXT("Empty cycle sequence fails"), PhaseComponent->TryGetCurrentPatternType(CurrentPatternType));

	// 사이클이 비어 있어도 진행 호출이 크래시하지 않습니다
	PhaseComponent->AdvancePatternCycle();
	TestEqual(TEXT("Empty cycle keeps index"), PhaseComponent->GetCurrentCycleIndex(), 0);

	// 후보가 없는 차례는 건너뛰어 보스가 그 차례에서 멈추지 않습니다
	TArray<FRSBossPhaseDefinition> SkipPhases;
	SkipPhases.Add(MakeTestPhase(BasicPattern, SpecialPattern, nullptr, 0.0f));
	SkipPhases[0].CycleSequence = { ERSBossPatternType::Basic, ERSBossPatternType::Special };
	SkipPhases[0].SpecialPatterns.Reset();

	PhaseComponent->SetPhasesForTest(SkipPhases);
	TestTrue(TEXT("Usable first turn resolves"), PhaseComponent->TryGetCurrentPatternType(CurrentPatternType));
	TestEqual(TEXT("First turn is basic"), CurrentPatternType, ERSBossPatternType::Basic);

	AddExpectedMessagePlain(TEXT("Skip 1 -> 0"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	PhaseComponent->AdvancePatternCycle();
	TestEqual(TEXT("Unusable special turn is skipped"), PhaseComponent->GetCurrentCycleIndex(), 0);
	TestTrue(TEXT("Skipped turn resolves"), PhaseComponent->TryGetCurrentPatternType(CurrentPatternType));
	TestEqual(TEXT("Turn returns to basic"), CurrentPatternType, ERSBossPatternType::Basic);

	// 모든 차례에 후보가 없으면 무한히 건너뛰지 않고 포기합니다
	TArray<FRSBossPhaseDefinition> AllUnusablePhases;
	AllUnusablePhases.Add(MakeTestPhase(BasicPattern, SpecialPattern, nullptr, 0.0f));
	AllUnusablePhases[0].CycleSequence = { ERSBossPatternType::Basic };
	AllUnusablePhases[0].BasicPatterns.Reset();

	PhaseComponent->SetPhasesForTest(AllUnusablePhases);
	AddExpectedMessagePlain(TEXT("Skip 0 -> 0"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	AddExpectedMessagePlain(TEXT("모든 차례에 후보가 없어"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	TestFalse(TEXT("All unusable turns give up"), PhaseComponent->SkipUnusablePatternTurns());
	TestEqual(TEXT("Give up keeps index"), PhaseComponent->GetCurrentCycleIndex(), 0);

	// 페이즈 데이터가 없으면 모든 조회가 안전하게 실패합니다
	URSBossPhaseComponent* UnconfiguredComponent = NewObject<URSBossPhaseComponent>();
	AddExpectedMessagePlain(TEXT("CycleSequence가 없어"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	TestEqual(TEXT("Unconfigured phase count"), UnconfiguredComponent->GetPhaseCount(), 0);
	TestFalse(TEXT("Unconfigured turn fails"), UnconfiguredComponent->TryGetCurrentPatternType(CurrentPatternType));

	return true;
}

#endif
