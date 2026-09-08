#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "CoreGlobals.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "RSBossEncounter.h"
#include "RSBossEncounterViewModel.h"
#include "RSGameModeBase.h"
#include "RSHealthComponent.h"
#include "RSPlayerCharacter.h"
#include "RSPlayerController.h"
#include "RSPlayerState.h"
#include "TimerManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSBossEncounterStateTest, "RS.Encounter.BossState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSBossEncounterStateTest::RunTest(const FString& Parameters)
{
	const TOptional<uint64> NoCandidateFrame;
	const TOptional<uint64> Frame100(100);
	const TOptional<uint64> Frame101(101);

	TestEqual(TEXT("No outcome candidates"), ARSBossEncounter::SelectOutcomeResult(NoCandidateFrame, NoCandidateFrame), ERSBossEncounterResult::None);
	TestEqual(TEXT("Boss-only candidate clears"), ARSBossEncounter::SelectOutcomeResult(Frame100, NoCandidateFrame), ERSBossEncounterResult::Clear);
	TestEqual(TEXT("Player-only candidate fails"), ARSBossEncounter::SelectOutcomeResult(NoCandidateFrame, Frame100), ERSBossEncounterResult::Failed);
	TestEqual(TEXT("Same-frame candidates clear"), ARSBossEncounter::SelectOutcomeResult(Frame100, Frame100), ERSBossEncounterResult::Clear);
	TestEqual(TEXT("Earlier boss candidate clears"), ARSBossEncounter::SelectOutcomeResult(Frame100, Frame101), ERSBossEncounterResult::Clear);
	TestEqual(TEXT("Earlier player candidate fails"), ARSBossEncounter::SelectOutcomeResult(Frame101, Frame100), ERSBossEncounterResult::Failed);

	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
	ARSBossEncounter* BossEncounter = TestWorld->SpawnActor<ARSBossEncounter>();
	URSBossEncounterViewModel* TimerViewModel = NewObject<URSBossEncounterViewModel>();
	TimerViewModel->InitializeViewModel(BossEncounter);

	TestEqual(TEXT("Initial state"), BossEncounter->GetEncounterState(), ERSBossEncounterState::Inactive);
	TestEqual(TEXT("Initial result"), BossEncounter->GetEncounterResult(), ERSBossEncounterResult::None);
	TestFalse(TEXT("Initial timer hidden"), TimerViewModel->bIsVisible);

	BossEncounter->StartEncounter();
	TestEqual(TEXT("Cannot start before preparing"), BossEncounter->GetEncounterState(), ERSBossEncounterState::Inactive);

	BossEncounter->BeginPreparing();
	TestEqual(TEXT("Preparing state"), BossEncounter->GetEncounterState(), ERSBossEncounterState::Preparing);
	TestEqual(TEXT("Preparing result"), BossEncounter->GetEncounterResult(), ERSBossEncounterResult::None);
	TestFalse(TEXT("Preparing timer hidden"), TimerViewModel->bIsVisible);

	BossEncounter->ResolveEncounter(ERSBossEncounterResult::Clear);
	TestEqual(TEXT("Cannot resolve before active"), BossEncounter->GetEncounterState(), ERSBossEncounterState::Preparing);

	AddExpectedError(TEXT("has no BossCharacter"), EAutomationExpectedErrorFlags::Contains, 1);
	BossEncounter->StartEncounter();
	TestEqual(TEXT("Active state"), BossEncounter->GetEncounterState(), ERSBossEncounterState::Active);
	TestEqual(TEXT("Active result"), BossEncounter->GetEncounterResult(), ERSBossEncounterResult::None);
	TestTrue(TEXT("Active timer scheduled"), TestWorld->GetTimerManager().IsTimerActive(BossEncounter->TimeLimitTimerHandle));
	TestTrue(TEXT("Started event refreshes timer visibility"), TimerViewModel->bIsVisible);
	TestTrue(TEXT("Started event enables timer tick"), TimerViewModel->IsTickable());

	BossEncounter->ResolveEncounter(static_cast<ERSBossEncounterResult>(MAX_uint8));
	TestEqual(TEXT("Invalid result keeps active state"), BossEncounter->GetEncounterState(), ERSBossEncounterState::Active);
	TestEqual(TEXT("Invalid result is not committed"), BossEncounter->GetEncounterResult(), ERSBossEncounterResult::None);

	BossEncounter->ResolveEncounter(ERSBossEncounterResult::Clear);
	TestEqual(TEXT("Clear state"), BossEncounter->GetEncounterState(), ERSBossEncounterState::Finished);
	TestEqual(TEXT("Clear result"), BossEncounter->GetEncounterResult(), ERSBossEncounterResult::Clear);
	TestFalse(TEXT("Finished timer stopped"), TestWorld->GetTimerManager().IsTimerActive(BossEncounter->TimeLimitTimerHandle));
	TestTrue(TEXT("Ended event keeps finished timer visible"), TimerViewModel->bIsVisible);
	TestFalse(TEXT("Ended event stops timer tick"), TimerViewModel->IsTickable());
	TestEqual(TEXT("Finished getter uses captured snapshot"), BossEncounter->GetRemainingTimeSeconds(), BossEncounter->CompletionRemainingTimeSeconds);
	TestEqual(TEXT("Finished event refreshes ViewModel result"), TimerViewModel->Result, ERSBossEncounterResult::Clear);
	TestEqual(TEXT("Finished event refreshes ViewModel title"), TimerViewModel->ResultTitle.ToString(), FString(TEXT("CLEAR")));
	TestFalse(TEXT("Finished event selects ViewModel message"), TimerViewModel->ResultMessage.IsEmpty());
	const FText ClearResultMessage = TimerViewModel->ResultMessage;
	TimerViewModel->InitializeViewModel(BossEncounter);
	TestTrue(TEXT("Same Source registration keeps Result Message"), TimerViewModel->ResultMessage.IdenticalTo(ClearResultMessage));

	URSBossEncounterViewModel* LateViewModel = NewObject<URSBossEncounterViewModel>();
	LateViewModel->InitializeViewModel(BossEncounter);
	TestEqual(TEXT("Late Source restores result"), LateViewModel->Result, ERSBossEncounterResult::Clear);
	TestEqual(TEXT("Late Source restores title"), LateViewModel->ResultTitle.ToString(), FString(TEXT("CLEAR")));
	TestFalse(TEXT("Late Source selects message"), LateViewModel->ResultMessage.IsEmpty());
	LateViewModel->UninitializeViewModel();

	BossEncounter->ResolveEncounter(ERSBossEncounterResult::Failed);
	TestEqual(TEXT("Duplicate resolve keeps state"), BossEncounter->GetEncounterState(), ERSBossEncounterState::Finished);
	TestEqual(TEXT("Duplicate resolve keeps first result"), BossEncounter->GetEncounterResult(), ERSBossEncounterResult::Clear);

	BossEncounter->ResetEncounter();
	TestEqual(TEXT("Finished reset state"), BossEncounter->GetEncounterState(), ERSBossEncounterState::Inactive);
	TestEqual(TEXT("Finished reset result"), BossEncounter->GetEncounterResult(), ERSBossEncounterResult::None);
	TestEqual(TEXT("Finished reset snapshot"), BossEncounter->GetRemainingTimeSeconds(), 0.0f);
	// 실제 런타임에서는 Source 해제로 초기화되며, 직접 연결한 단위 테스트는 같은 Source 재동기화를 명시합니다
	TimerViewModel->InitializeViewModel(BossEncounter);
	TestEqual(TEXT("Finished reset clears ViewModel result"), TimerViewModel->Result, ERSBossEncounterResult::None);
	TestTrue(TEXT("Finished reset clears ViewModel message"), TimerViewModel->ResultMessage.IsEmpty());

	TimerViewModel->UninitializeViewModel();
	TimerViewModel->InitializeViewModel(BossEncounter);
	BossEncounter->BeginPreparing();
	AddExpectedError(TEXT("has no BossCharacter"), EAutomationExpectedErrorFlags::Contains, 1);
	BossEncounter->StartEncounter();
	BossEncounter->ResetEncounter();
	TestEqual(TEXT("Active reset state"), BossEncounter->GetEncounterState(), ERSBossEncounterState::Inactive);
	TestEqual(TEXT("Active reset result"), BossEncounter->GetEncounterResult(), ERSBossEncounterResult::None);
	TestFalse(TEXT("Active reset Ended event hides timer"), TimerViewModel->bIsVisible);
	TestFalse(TEXT("Active reset Ended event stops timer tick"), TimerViewModel->IsTickable());

	BossEncounter->BeginPreparing();
	AddExpectedError(TEXT("has no BossCharacter"), EAutomationExpectedErrorFlags::Contains, 1);
	BossEncounter->StartEncounter();
	BossEncounter->ResolveEncounter(ERSBossEncounterResult::Failed);
	TestEqual(TEXT("Failed state"), BossEncounter->GetEncounterState(), ERSBossEncounterState::Finished);
	TestEqual(TEXT("Failed result"), BossEncounter->GetEncounterResult(), ERSBossEncounterResult::Failed);

	BossEncounter->ResetEncounter();
	ARSPlayerState* FirstParticipant = TestWorld->SpawnActor<ARSPlayerState>();
	ARSPlayerCharacter* FirstPlayerCharacter = TestWorld->SpawnActor<ARSPlayerCharacter>();
	FirstPlayerCharacter->SetPlayerState(FirstParticipant);
	URSHealthComponent* FirstHealthComponent = FirstPlayerCharacter->GetHealthComponent();

	BossEncounter->BeginPreparing();
	BossEncounter->RegisterParticipant(FirstParticipant);
	TestEqual(TEXT("Preparing participant death observation is deferred"), BossEncounter->ParticipantDeathHealthComponents.Num(), 0);

	AddExpectedError(TEXT("has no BossCharacter"), EAutomationExpectedErrorFlags::Contains, 1);
	BossEncounter->StartEncounter();
	TestEqual(TEXT("Preparing participant binds on active start"), BossEncounter->ParticipantDeathHealthComponents.Num(), 1);
	TestEqual(TEXT("First participant uses actual health component"), BossEncounter->ParticipantDeathHealthComponents.FindRef(FirstParticipant).Get(), FirstHealthComponent);
	TestTrue(TEXT("First participant death delegate is bound"), FirstHealthComponent->OnDeathStarted.IsAlreadyBound(BossEncounter, &ARSBossEncounter::HandleParticipantDeathStarted));

	BossEncounter->RegisterParticipant(FirstParticipant);
	TestEqual(TEXT("Duplicate participant does not duplicate death observation"), BossEncounter->ParticipantDeathHealthComponents.Num(), 1);

	ARSPlayerState* SecondParticipant = TestWorld->SpawnActor<ARSPlayerState>();
	ARSPlayerCharacter* SecondPlayerCharacter = TestWorld->SpawnActor<ARSPlayerCharacter>();
	SecondPlayerCharacter->SetPlayerState(SecondParticipant);
	URSHealthComponent* SecondHealthComponent = SecondPlayerCharacter->GetHealthComponent();
	BossEncounter->RegisterParticipant(SecondParticipant);
	TestEqual(TEXT("Active participant binds immediately"), BossEncounter->ParticipantDeathHealthComponents.Num(), 2);
	TestEqual(TEXT("Second participant uses actual health component"), BossEncounter->ParticipantDeathHealthComponents.FindRef(SecondParticipant).Get(), SecondHealthComponent);

	BossEncounter->UnregisterParticipant(SecondParticipant);
	TestEqual(TEXT("Participant removal unbinds death observation"), BossEncounter->ParticipantDeathHealthComponents.Num(), 1);
	TestEqual(TEXT("First participant observation remains after second removal"), BossEncounter->ParticipantDeathHealthComponents.FindRef(FirstParticipant).Get(), FirstHealthComponent);
	TestTrue(TEXT("First participant delegate remains after second removal"), FirstHealthComponent->OnDeathStarted.IsAlreadyBound(BossEncounter, &ARSBossEncounter::HandleParticipantDeathStarted));
	TestFalse(TEXT("Removed participant death delegate is unbound"), SecondHealthComponent->OnDeathStarted.IsAlreadyBound(BossEncounter, &ARSBossEncounter::HandleParticipantDeathStarted));

	TestEqual(TEXT("Death callback scenario remains active"), BossEncounter->GetEncounterState(), ERSBossEncounterState::Active);
	TestTrue(TEXT("Death callback health component is valid"), IsValid(FirstHealthComponent));
	BossEncounter->HandleParticipantDeathStarted(FirstHealthComponent);
	TestTrue(TEXT("Player death schedules outcome evaluation"), BossEncounter->bIsOutcomeEvaluationPending);
	TestTrue(TEXT("Player death records failed candidate"), BossEncounter->FailedCandidateFrame.IsSet());
	TestTrue(TEXT("Outcome evaluation timer is valid"), BossEncounter->OutcomeEvaluationTimerHandle.IsValid());
	if (!BossEncounter->FailedCandidateFrame.IsSet())
	{
		TimerViewModel->UninitializeViewModel();
		TestWorld->DestroyWorld(false);
		return false;
	}

	const uint64 FirstFailedCandidateFrame = BossEncounter->FailedCandidateFrame.GetValue();
	const FTimerHandle FirstEvaluationTimerHandle = BossEncounter->OutcomeEvaluationTimerHandle;

	BossEncounter->HandleParticipantDeathStarted(FirstHealthComponent);
	TestEqual(TEXT("Duplicate player death preserves first frame"), BossEncounter->FailedCandidateFrame.GetValue(), FirstFailedCandidateFrame);
	TestTrue(TEXT("Duplicate player death preserves one evaluation"), BossEncounter->OutcomeEvaluationTimerHandle == FirstEvaluationTimerHandle);

	BossEncounter->RequestClearOutcome();
	TestTrue(TEXT("Boss death records clear candidate"), BossEncounter->ClearCandidateFrame.IsSet());
	if (!BossEncounter->ClearCandidateFrame.IsSet())
	{
		TimerViewModel->UninitializeViewModel();
		TestWorld->DestroyWorld(false);
		return false;
	}

	TestEqual(TEXT("Same-frame integration candidates"), BossEncounter->ClearCandidateFrame.GetValue(), BossEncounter->FailedCandidateFrame.GetValue());
	BossEncounter->EvaluateOutcome();
	TestEqual(TEXT("Same-frame evaluation waits for a later engine frame"), BossEncounter->GetEncounterState(), ERSBossEncounterState::Active);
	TestTrue(TEXT("Same-frame evaluation is rescheduled"), BossEncounter->bIsOutcomeEvaluationPending);
	TestTrue(TEXT("Automation frame counter can provide an earlier frame"), GFrameCounter > 0);
	if (GFrameCounter == 0)
	{
		TimerViewModel->UninitializeViewModel();
		TestWorld->DestroyWorld(false);
		return false;
	}

	BossEncounter->ClearCandidateFrame.Emplace(GFrameCounter - 1);
	BossEncounter->FailedCandidateFrame.Emplace(GFrameCounter - 1);
	BossEncounter->EvaluateOutcome();
	TestEqual(TEXT("Same-frame integration result clears"), BossEncounter->GetEncounterResult(), ERSBossEncounterResult::Clear);
	TestEqual(TEXT("Finished cleanup removes death observations"), BossEncounter->ParticipantDeathHealthComponents.Num(), 0);
	TestFalse(TEXT("Finished cleanup unbinds participant delegate"), FirstHealthComponent->OnDeathStarted.IsAlreadyBound(BossEncounter, &ARSBossEncounter::HandleParticipantDeathStarted));
	TestFalse(TEXT("Finished cleanup removes pending evaluation"), BossEncounter->bIsOutcomeEvaluationPending);
	TestFalse(TEXT("Finished cleanup invalidates evaluation timer"), BossEncounter->OutcomeEvaluationTimerHandle.IsValid());

	BossEncounter->ResetEncounter();
	AddExpectedError(TEXT("has no BossCharacter"), EAutomationExpectedErrorFlags::Contains, 1);
	BossEncounter->RegisterParticipant(FirstParticipant);
	BossEncounter->HandleParticipantDeathStarted(FirstHealthComponent);
	BossEncounter->FailedCandidateFrame.Emplace(GFrameCounter - 1);
	BossEncounter->EvaluateOutcome();
	TestEqual(TEXT("Player-only integration result fails"), BossEncounter->GetEncounterResult(), ERSBossEncounterResult::Failed);

	BossEncounter->ResetEncounter();
	AddExpectedError(TEXT("has no BossCharacter"), EAutomationExpectedErrorFlags::Contains, 1);
	BossEncounter->RegisterParticipant(FirstParticipant);
	BossEncounter->RequestClearOutcome();
	BossEncounter->ClearCandidateFrame.Emplace(GFrameCounter - 1);
	BossEncounter->EvaluateOutcome();
	TestEqual(TEXT("Boss-only integration result clears"), BossEncounter->GetEncounterResult(), ERSBossEncounterResult::Clear);

	BossEncounter->ResetEncounter();
	AddExpectedError(TEXT("has no BossCharacter"), EAutomationExpectedErrorFlags::Contains, 1);
	BossEncounter->RegisterParticipant(FirstParticipant);
	BossEncounter->HandleParticipantDeathStarted(FirstHealthComponent);
	TestTrue(TEXT("Reset scenario has pending evaluation"), BossEncounter->bIsOutcomeEvaluationPending);
	BossEncounter->ResetEncounter();
	TestFalse(TEXT("Reset clears pending evaluation"), BossEncounter->bIsOutcomeEvaluationPending);
	TestFalse(TEXT("Reset clears failed candidate"), BossEncounter->FailedCandidateFrame.IsSet());
	TestFalse(TEXT("Reset invalidates evaluation timer"), BossEncounter->OutcomeEvaluationTimerHandle.IsValid());
	TestEqual(TEXT("Reset removes death observations"), BossEncounter->ParticipantDeathHealthComponents.Num(), 0);
	BossEncounter->EvaluateOutcome();
	TestEqual(TEXT("Stale evaluation cannot change reset state"), BossEncounter->GetEncounterState(), ERSBossEncounterState::Inactive);
	TestEqual(TEXT("Stale evaluation cannot set reset result"), BossEncounter->GetEncounterResult(), ERSBossEncounterResult::None);

	ARSGameModeBase* GameMode = TestWorld->SpawnActor<ARSGameModeBase>();
	ARSPlayerController* PlayerController = TestWorld->SpawnActor<ARSPlayerController>();
	ULocalPlayer* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
	PlayerController->SetPlayer(LocalPlayer);
	TestTrue(TEXT("Result flow test PlayerController is local"), PlayerController->IsLocalController());

	GameMode->BindBossEncounter();
	ARSBossEncounter* BoundBossEncounter = GameMode->BoundBossEncounter.Get();
	TestNotNull(TEXT("GameMode discovers a Boss Encounter"), BoundBossEncounter);
	if (!BoundBossEncounter)
	{
		TimerViewModel->UninitializeViewModel();
		TestWorld->DestroyWorld(false);
		return false;
	}

	TestTrue(
		TEXT("GameMode binds the Encounter finished output"),
		BoundBossEncounter->OnEncounterFinished.IsAlreadyBound(GameMode, &ARSGameModeBase::HandleBossEncounterFinished));

	ARSBossEncounter* ForeignBossEncounter = TestWorld->SpawnActor<ARSBossEncounter>();
	GameMode->HandleBossEncounterFinished(ForeignBossEncounter, ERSBossEncounterResult::Clear);
	TestFalse(TEXT("Foreign Encounter cannot start Result Flow"), GameMode->HasBossResultFlowStarted());
	TestFalse(TEXT("Foreign Encounter is not delivered to local presentation"), PlayerController->HasBossResultPresentationStarted());

	GameMode->HandleBossEncounterFinished(BoundBossEncounter, ERSBossEncounterResult::None);
	TestFalse(TEXT("Invalid result cannot start Result Flow"), GameMode->HasBossResultFlowStarted());

	GameMode->HandleBossEncounterFinished(BoundBossEncounter, ERSBossEncounterResult::Clear);
	TestTrue(TEXT("Clear starts GameMode Result Flow"), GameMode->HasBossResultFlowStarted());
	TestEqual(TEXT("GameMode preserves Clear result"), GameMode->GetBossResult(), ERSBossEncounterResult::Clear);
	TestTrue(TEXT("Clear enters local Result Presentation boundary"), PlayerController->HasBossResultPresentationStarted());
	TestEqual(TEXT("PlayerController receives Clear result"), PlayerController->GetBossResultPresentation(), ERSBossEncounterResult::Clear);

	GameMode->HandleBossEncounterFinished(BoundBossEncounter, ERSBossEncounterResult::Failed);
	TestEqual(TEXT("Duplicate Result Flow keeps first GameMode result"), GameMode->GetBossResult(), ERSBossEncounterResult::Clear);
	TestEqual(TEXT("Duplicate Result Flow keeps first presentation result"), PlayerController->GetBossResultPresentation(), ERSBossEncounterResult::Clear);

	GameMode->UnbindBossEncounter();
	TestFalse(
		TEXT("GameMode removes the Encounter finished output binding"),
		BoundBossEncounter->OnEncounterFinished.IsAlreadyBound(GameMode, &ARSGameModeBase::HandleBossEncounterFinished));
	TestFalse(TEXT("Unbind clears the bound Encounter reference"), GameMode->BoundBossEncounter.IsValid());

	GameMode->BoundBossEncounter = BoundBossEncounter;
	GameMode->BossResult.Reset();
	PlayerController->BossResultPresentation.Reset();
	GameMode->HandleBossEncounterFinished(BoundBossEncounter, ERSBossEncounterResult::Failed);
	TestEqual(TEXT("Failed starts a fresh GameMode Result Flow"), GameMode->GetBossResult(), ERSBossEncounterResult::Failed);
	TestEqual(TEXT("PlayerController receives Failed result"), PlayerController->GetBossResultPresentation(), ERSBossEncounterResult::Failed);

	GameMode->BossResultAction.Reset();
	GameMode->BossResult.Reset();
	TestFalse(TEXT("Result Action requires a started Result Flow"), GameMode->TryCommitBossResultAction(ERSBossResultAction::RestartLevel));

	GameMode->BossResult.Emplace(ERSBossEncounterResult::Clear);
	TestFalse(
		TEXT("Unknown Result Action cannot be committed"),
		GameMode->TryCommitBossResultAction(static_cast<ERSBossResultAction>(MAX_uint8)));
	TestTrue(TEXT("Restart Level Action commits after Clear"), GameMode->TryCommitBossResultAction(ERSBossResultAction::RestartLevel));
	TestTrue(TEXT("Committed Result Action is in progress"), GameMode->IsBossResultActionInProgress());
	TestEqual(TEXT("Committed Result Action is preserved"), GameMode->GetBossResultAction().GetValue(), ERSBossResultAction::RestartLevel);
	TestFalse(TEXT("Second Result Action is rejected"), GameMode->TryCommitBossResultAction(ERSBossResultAction::ReturnToMainMenu));

	GameMode->BossResultAction.Reset();
	GameMode->BossResult.Emplace(ERSBossEncounterResult::Failed);
	TestTrue(TEXT("Return to Main Menu Action commits after Failed"), GameMode->TryCommitBossResultAction(ERSBossResultAction::ReturnToMainMenu));
	TestEqual(TEXT("Main Menu Action is preserved"), GameMode->GetBossResultAction().GetValue(), ERSBossResultAction::ReturnToMainMenu);

	TimerViewModel->UninitializeViewModel();
	TestWorld->DestroyWorld(false);
	return true;
}

#endif
