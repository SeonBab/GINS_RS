// Fill out your copyright notice in the Description page of Project Settings.


#include "RSBossEncounterTimerViewModel.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "RSBossEncounter.h"

void URSBossEncounterTimerViewModel::Tick(float)
{
	// Encounter가 제거되면 종료나 만료 이벤트가 오지 않으므로 여기서 연결을 정리해 갱신을 멈춥니다
	if (!BossEncounter.IsValid())
	{
		UninitializeViewModel();
		return;
	}

	RefreshFromSource();
}

bool URSBossEncounterTimerViewModel::IsTickable() const
{
	return bIsTimerRunning;
}

TStatId URSBossEncounterTimerViewModel::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URSBossEncounterTimerViewModel, STATGROUP_Tickables);
}

UWorld* URSBossEncounterTimerViewModel::GetTickableGameObjectWorld() const
{
	// LocalPlayer 저장소가 소유하므로 Outer를 따라 올라가야 월드에 닿습니다
	return GEngine ? GEngine->GetWorldFromContextObject(this, EGetWorldErrorMode::ReturnNull) : nullptr;
}

void URSBossEncounterTimerViewModel::BeginDestroy()
{
	// 제거 중인 ViewModel이 Tick되지 않도록 값 통지 없이 갱신만 멈춥니다
	bIsTimerRunning = false;

	DisconnectBossEncounter();

	Super::BeginDestroy();
}

void URSBossEncounterTimerViewModel::HandleSourceRegistered(UObject* Source)
{
	ARSBossEncounter* SourceEncounter = Cast<ARSBossEncounter>(Source);
	if (!IsValid(SourceEncounter))
	{
		return;
	}

	InitializeViewModel(SourceEncounter);
}

void URSBossEncounterTimerViewModel::HandleSourceUnregistered(UObject* Source)
{
	const ARSBossEncounter* SourceEncounter = Cast<ARSBossEncounter>(Source);
	if (!SourceEncounter || BossEncounter.Get() != SourceEncounter)
	{
		return;
	}

	UninitializeViewModel();
}

void URSBossEncounterTimerViewModel::InitializeViewModel(ARSBossEncounter* InBossEncounter)
{
	// 같은 원본을 다시 등록받으면 재구독하지 않고 현재 상태만 다시 읽습니다
	if (IsValid(InBossEncounter) && BossEncounter.Get() == InBossEncounter)
	{
		RefreshFromSource();
		return;
	}

	DisconnectBossEncounter();

	if (!IsValid(InBossEncounter))
	{
		ResetTimerValues();
		return;
	}

	BossEncounter = InBossEncounter;
	InBossEncounter->OnEncounterStarted.AddUniqueDynamic(this, &ThisClass::HandleEncounterChanged);
	InBossEncounter->OnEncounterEnded.AddUniqueDynamic(this, &ThisClass::HandleEncounterChanged);
	InBossEncounter->OnTimeLimitExpired.AddUniqueDynamic(this, &ThisClass::HandleEncounterChanged);

	// 이미 진행 중이거나 완료된 전투가 원본으로 들어와도 현재 값을 복구할 수 있도록 즉시 동기화합니다
	RefreshFromSource();
}

void URSBossEncounterTimerViewModel::UninitializeViewModel()
{
	DisconnectBossEncounter();
	ResetTimerValues();
}

void URSBossEncounterTimerViewModel::HandleEncounterChanged(ARSBossEncounter* InBossEncounter)
{
	if (BossEncounter.Get() != InBossEncounter)
	{
		return;
	}

	RefreshFromSource();
}

void URSBossEncounterTimerViewModel::RefreshFromSource()
{
	const ARSBossEncounter* CurrentEncounter = BossEncounter.Get();
	if (!IsValid(CurrentEncounter))
	{
		ResetTimerValues();
		return;
	}

	ApplyEncounterValues(CurrentEncounter->GetEncounterState(), CurrentEncounter->HasTimeLimitExpired(), CurrentEncounter->GetRemainingTimeSeconds());
}

void URSBossEncounterTimerViewModel::ApplyEncounterValues(ERSBossEncounterState InEncounterState, bool bInHasTimeLimitExpired, float InRemainingSeconds)
{
	// 시작되지 않았거나 초기화된 전투에는 표시할 남은 시간이라는 개념이 없습니다
	if (InEncounterState == ERSBossEncounterState::Inactive)
	{
		ResetTimerValues();
		return;
	}

	// 완료된 전투는 고정된 값을, 만료된 제한 시간은 0을 표시하므로 연속 갱신이 필요하지 않습니다
	bIsTimerRunning = InEncounterState == ERSBossEncounterState::Active && !bInHasTimeLimitExpired;

	// 실제로 시간이 남아 있는데 먼저 00:00으로 보이지 않도록 올림으로 표시합니다
	const int32 NewDisplayedSeconds = FMath::Max(FMath::CeilToInt(InRemainingSeconds), 0);

	// FText는 내용이 같아도 새로 만들면 다른 값으로 취급되므로 표시 초가 바뀔 때만 다시 만듭니다
	if (NewDisplayedSeconds != DisplayedSeconds)
	{
		DisplayedSeconds = NewDisplayedSeconds;
		UE_MVVM_SET_PROPERTY_VALUE(RemainingTimeText, MakeRemainingTimeText(NewDisplayedSeconds));
	}

	// 표시 여부는 연결 성공이 아니라 전투가 시작되었는지로 결정합니다
	UE_MVVM_SET_PROPERTY_VALUE(bIsVisible, true);
}

void URSBossEncounterTimerViewModel::ResetTimerValues()
{
	bIsTimerRunning = false;

	// 같은 초가 다시 들어와도 문자열을 새로 만들도록 표시 이력을 비웁니다
	DisplayedSeconds = -1;

	// 값이 없는 상태를 00:00과 구분하기 위해 빈 문자열로 둡니다
	UE_MVVM_SET_PROPERTY_VALUE(RemainingTimeText, FText::GetEmpty());
	UE_MVVM_SET_PROPERTY_VALUE(bIsVisible, false);
}

void URSBossEncounterTimerViewModel::DisconnectBossEncounter()
{
	if (ARSBossEncounter* CurrentEncounter = BossEncounter.Get())
	{
		CurrentEncounter->OnEncounterStarted.RemoveDynamic(this, &ThisClass::HandleEncounterChanged);
		CurrentEncounter->OnEncounterEnded.RemoveDynamic(this, &ThisClass::HandleEncounterChanged);
		CurrentEncounter->OnTimeLimitExpired.RemoveDynamic(this, &ThisClass::HandleEncounterChanged);
	}

	BossEncounter.Reset();
}

FText URSBossEncounterTimerViewModel::MakeRemainingTimeText(int32 InTotalSeconds)
{
	const int32 SafeTotalSeconds = FMath::Max(InTotalSeconds, 0);

	// 한 시간을 넘는 제한 시간도 시간 단위를 만들지 않고 분 자리를 늘려 표시합니다
	return FText::AsCultureInvariant(FString::Printf(TEXT("%02d:%02d"), SafeTotalSeconds / 60, SafeTotalSeconds % 60));
}
