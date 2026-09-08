// Fill out your copyright notice in the Description page of Project Settings.

#include "RSBossEncounterViewModel.h"

#include "Engine/Engine.h"
#include "Engine/World.h"

void URSBossEncounterViewModel::Tick(float)
{
	// Encounter가 제거되면 종료나 만료 이벤트가 오지 않으므로 여기서 연결을 정리해 갱신을 멈춥니다
	if (!BossEncounter.IsValid())
	{
		UninitializeViewModel();
		return;
	}

	RefreshFromSource();
}

bool URSBossEncounterViewModel::IsTickable() const
{
	return bIsTimerRunning;
}

TStatId URSBossEncounterViewModel::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URSBossEncounterViewModel, STATGROUP_Tickables);
}

UWorld* URSBossEncounterViewModel::GetTickableGameObjectWorld() const
{
	// LocalPlayer 저장소가 소유하므로 Outer를 따라 올라가야 월드에 닿습니다
	return GEngine ? GEngine->GetWorldFromContextObject(this, EGetWorldErrorMode::ReturnNull) : nullptr;
}

void URSBossEncounterViewModel::BeginDestroy()
{
	bIsTimerRunning = false;
	DisconnectBossEncounter();

	Super::BeginDestroy();
}

void URSBossEncounterViewModel::HandleSourceRegistered(UObject* Source)
{
	ARSBossEncounter* SourceEncounter = Cast<ARSBossEncounter>(Source);
	if (!IsValid(SourceEncounter))
	{
		return;
	}

	InitializeViewModel(SourceEncounter);
}

void URSBossEncounterViewModel::HandleSourceUnregistered(UObject* Source)
{
	const ARSBossEncounter* SourceEncounter = Cast<ARSBossEncounter>(Source);
	if (!SourceEncounter || BossEncounter.Get() != SourceEncounter)
	{
		return;
	}

	UninitializeViewModel();
}

void URSBossEncounterViewModel::InitializeViewModel(ARSBossEncounter* InBossEncounter)
{
	// 같은 Source 재등록은 현재 Result Cycle의 Message를 유지한 채 값만 다시 동기화합니다
	if (IsValid(InBossEncounter) && BossEncounter.Get() == InBossEncounter)
	{
		RefreshFromSource();
		return;
	}

	DisconnectBossEncounter();
	ResetEncounterValues();

	if (!IsValid(InBossEncounter))
	{
		return;
	}

	BossEncounter = InBossEncounter;
	InBossEncounter->OnEncounterStarted.AddUniqueDynamic(this, &ThisClass::HandleEncounterChanged);
	InBossEncounter->OnEncounterEnded.AddUniqueDynamic(this, &ThisClass::HandleEncounterChanged);
	InBossEncounter->OnEncounterFinished.AddUniqueDynamic(this, &ThisClass::HandleEncounterFinished);
	InBossEncounter->OnTimeLimitExpired.AddUniqueDynamic(this, &ThisClass::HandleEncounterChanged);

	// 이미 진행 중이거나 완료된 전투가 늦게 등록되어도 현재 Snapshot을 즉시 복구합니다
	RefreshFromSource();
}

void URSBossEncounterViewModel::UninitializeViewModel()
{
	DisconnectBossEncounter();
	ResetEncounterValues();
}

void URSBossEncounterViewModel::HandleEncounterChanged(ARSBossEncounter* InBossEncounter)
{
	if (BossEncounter.Get() != InBossEncounter)
	{
		return;
	}

	RefreshFromSource();
}

void URSBossEncounterViewModel::HandleEncounterFinished(ARSBossEncounter* InBossEncounter, ERSBossEncounterResult InResult)
{
	if (BossEncounter.Get() != InBossEncounter || InResult != InBossEncounter->GetEncounterResult())
	{
		return;
	}

	RefreshFromSource();
}

void URSBossEncounterViewModel::RefreshFromSource()
{
	const ARSBossEncounter* CurrentEncounter = BossEncounter.Get();
	if (!IsValid(CurrentEncounter))
	{
		ResetEncounterValues();
		return;
	}

	const ERSBossEncounterState EncounterState = CurrentEncounter->GetEncounterState();
	ApplyTimerValues(EncounterState, CurrentEncounter->HasTimeLimitExpired(), CurrentEncounter->GetRemainingTimeSeconds());
	ApplyResultValues(EncounterState, CurrentEncounter->GetEncounterResult());
}

void URSBossEncounterViewModel::ApplyTimerValues(ERSBossEncounterState InEncounterState, bool bInHasTimeLimitExpired, float InRemainingSeconds)
{
	if (InEncounterState == ERSBossEncounterState::Inactive || InEncounterState == ERSBossEncounterState::Preparing)
	{
		ResetTimerValues();
		return;
	}

	bIsTimerRunning = InEncounterState == ERSBossEncounterState::Active && !bInHasTimeLimitExpired;

	const int32 NewDisplayedSeconds = FMath::Max(FMath::CeilToInt(InRemainingSeconds), 0);
	if (NewDisplayedSeconds != DisplayedSeconds)
	{
		DisplayedSeconds = NewDisplayedSeconds;
		UE_MVVM_SET_PROPERTY_VALUE(RemainingTimeText, MakeRemainingTimeText(NewDisplayedSeconds));
	}

	UE_MVVM_SET_PROPERTY_VALUE(bIsVisible, true);
}

void URSBossEncounterViewModel::ApplyResultValues(ERSBossEncounterState InEncounterState, ERSBossEncounterResult InResult)
{
	const bool bHasFinishedResult = InEncounterState == ERSBossEncounterState::Finished &&
		(InResult == ERSBossEncounterResult::Clear || InResult == ERSBossEncounterResult::Failed);
	if (!bHasFinishedResult)
	{
		ResetResultValues();
		return;
	}

	const bool bNeedsNewMessage = !bHasSelectedResultMessage || Result != InResult;
	if (bNeedsNewMessage)
	{
		bHasSelectedResultMessage = true;
		UE_MVVM_SET_PROPERTY_VALUE(ResultMessage, SelectResultMessage(InResult));
	}

	UE_MVVM_SET_PROPERTY_VALUE(ResultTitle, MakeResultTitle(InResult));
	UE_MVVM_SET_PROPERTY_VALUE(Result, InResult);
}

void URSBossEncounterViewModel::ResetEncounterValues()
{
	ResetTimerValues();
	ResetResultValues();
}

void URSBossEncounterViewModel::ResetTimerValues()
{
	bIsTimerRunning = false;
	DisplayedSeconds = -1;

	UE_MVVM_SET_PROPERTY_VALUE(RemainingTimeText, FText::GetEmpty());
	UE_MVVM_SET_PROPERTY_VALUE(bIsVisible, false);
}

void URSBossEncounterViewModel::ResetResultValues()
{
	bHasSelectedResultMessage = false;

	UE_MVVM_SET_PROPERTY_VALUE(Result, ERSBossEncounterResult::None);
	UE_MVVM_SET_PROPERTY_VALUE(ResultTitle, FText::GetEmpty());
	UE_MVVM_SET_PROPERTY_VALUE(ResultMessage, FText::GetEmpty());
}

void URSBossEncounterViewModel::DisconnectBossEncounter()
{
	if (ARSBossEncounter* CurrentEncounter = BossEncounter.Get())
	{
		CurrentEncounter->OnEncounterStarted.RemoveDynamic(this, &ThisClass::HandleEncounterChanged);
		CurrentEncounter->OnEncounterEnded.RemoveDynamic(this, &ThisClass::HandleEncounterChanged);
		CurrentEncounter->OnEncounterFinished.RemoveDynamic(this, &ThisClass::HandleEncounterFinished);
		CurrentEncounter->OnTimeLimitExpired.RemoveDynamic(this, &ThisClass::HandleEncounterChanged);
	}

	BossEncounter.Reset();
}

FText URSBossEncounterViewModel::MakeResultTitle(ERSBossEncounterResult InResult)
{
	switch (InResult)
	{
	case ERSBossEncounterResult::Clear:
		return NSLOCTEXT("RSBossEncounterResult", "ClearTitle", "CLEAR");

	case ERSBossEncounterResult::Failed:
		return NSLOCTEXT("RSBossEncounterResult", "FailedTitle", "FAILED");

	default:
		return FText::GetEmpty();
	}
}

FText URSBossEncounterViewModel::SelectResultMessage(ERSBossEncounterResult InResult)
{
	const TConstArrayView<FText> Candidates = GetResultMessageCandidates(InResult);
	return Candidates.IsEmpty() ? FText::GetEmpty() : Candidates[FMath::RandHelper(Candidates.Num())];
}

TConstArrayView<FText> URSBossEncounterViewModel::GetResultMessageCandidates(ERSBossEncounterResult InResult)
{
	static const TArray<FText> ClearMessages =
	{
		NSLOCTEXT("RSBossEncounterResult", "ClearMessage01", "보스를 쓰러뜨렸다."),
		NSLOCTEXT("RSBossEncounterResult", "ClearMessage02", "승리를 거두었다.")
	};

	static const TArray<FText> FailedMessages =
	{
		NSLOCTEXT("RSBossEncounterResult", "FailedMessage01", "플레이에 도움이 되는 팁1"),
		NSLOCTEXT("RSBossEncounterResult", "FailedMessage02", "플레이에 도움이 되는 팁2"),
		NSLOCTEXT("RSBossEncounterResult", "FailedMessage03", "플레이에 도움이 되는 팁3")
	};

	if (InResult == ERSBossEncounterResult::Clear)
	{
		return ClearMessages;
	}

	if (InResult == ERSBossEncounterResult::Failed)
	{
		return FailedMessages;
	}

	return TConstArrayView<FText>();
}

FText URSBossEncounterViewModel::MakeRemainingTimeText(int32 InTotalSeconds)
{
	const int32 SafeTotalSeconds = FMath::Max(InTotalSeconds, 0);
	return FText::AsCultureInvariant(FString::Printf(TEXT("%02d:%02d"), SafeTotalSeconds / 60, SafeTotalSeconds % 60));
}
