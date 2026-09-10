// Fill out your copyright notice in the Description page of Project Settings.


#include "RSBossPhaseComponent.h"

#include "GameFramework/Pawn.h"
#include "Abilities/RSBaseGameplayAbility.h"
#include "RSBossPhaseData.h"
#include "RSHealthComponent.h"

namespace
{
	/** 로그에서 사이클 진행을 추적할 수 있도록 패턴 종류의 이름을 반환합니다 */
	FString GetPatternTypeName(ERSBossPatternType PatternType)
	{
		return UEnum::GetDisplayValueAsText(PatternType).ToString();
	}
}

URSBossPhaseComponent::URSBossPhaseComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void URSBossPhaseComponent::BeginPlay()
{
	Super::BeginPlay();

	URSHealthComponent* HealthComponent = FindHealthComponent();
	if (!HealthComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BossPhase] %s에 HealthComponent가 없어 페이즈 트리거를 감시할 수 없습니다"), *GetNameSafe(GetOwner()));

		return;
	}

	ObservedHealthComponent = HealthComponent;
	HealthComponent->OnHealthChanged.AddUniqueDynamic(this, &ThisClass::HandleHealthChanged);

	// 첫 차례부터 후보가 없을 수 있으므로 전투 시작 전에 실행 가능한 차례로 맞춥니다
	SkipUnusablePatternTurns();

	// ASC 초기화가 이 시점보다 앞설 수 있으므로 구독 직후 현재 체력으로 한 번 평가합니다
	EvaluateHealthTrigger(HealthComponent->GetHealth(), HealthComponent->GetMaxHealth());
}

void URSBossPhaseComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ObservedHealthComponent)
	{
		ObservedHealthComponent->OnHealthChanged.RemoveDynamic(this, &ThisClass::HandleHealthChanged);
		ObservedHealthComponent = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

URSBossPhaseComponent* URSBossPhaseComponent::FindPhaseComponent(const APawn* Pawn)
{
	// Behavior Tree 노드가 보스 캐릭터 타입을 알지 않아도 진행 컴포넌트를 찾을 수 있게 합니다
	return Pawn ? Pawn->FindComponentByClass<URSBossPhaseComponent>() : nullptr;
}

#pragma region Pattern Cycle

bool URSBossPhaseComponent::TryGetCurrentPatternType(ERSBossPatternType& OutPatternType) const
{
	const FRSBossPhaseDefinition* CurrentPhase = GetCurrentPhase();
	if (!CurrentPhase || !CurrentPhase->CycleSequence.IsValidIndex(CurrentCycleIndex))
	{
		// 사이클이 비어 있으면 어떤 차례도 성립하지 않으므로 런타임 상황이 아니라 에셋 설정 누락입니다
		UE_LOG(LogTemp, Warning, TEXT("[BossPhase] %s의 %d번 페이즈에 CycleSequence가 없어 현재 차례를 결정할 수 없습니다"), *GetNameSafe(GetOwner()), CurrentPhaseIndex);

		return false;
	}

	OutPatternType = CurrentPhase->CycleSequence[CurrentCycleIndex];

	return true;
}

bool URSBossPhaseComponent::TrySelectPattern(ERSBossPatternType PatternType, TSubclassOf<URSBaseGameplayAbility>& OutAbilityClass) const
{
	const TArray<TSubclassOf<URSBaseGameplayAbility>>* Candidates = GetPatternCandidates(PatternType);

	// 후보가 없는 것은 정상적인 실행 실패가 아니라 에셋 설정 누락입니다
	if (!Candidates || Candidates->IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[BossPhase] %s의 %d번 페이즈에 %s 패턴 후보가 없습니다"), *GetNameSafe(GetOwner()), CurrentPhaseIndex, *GetPatternTypeName(PatternType));

		return false;
	}

	// 1단계에서는 직전 패턴 제외와 쿨다운 필터를 두지 않으므로 후보 중 하나를 그대로 고릅니다
	const int32 SelectedIndex = FMath::RandRange(0, Candidates->Num() - 1);
	const TSubclassOf<URSBaseGameplayAbility> SelectedAbilityClass = (*Candidates)[SelectedIndex];
	if (!SelectedAbilityClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BossPhase] %s의 %d번 페이즈 %s 후보 %d번이 비어 있습니다"), *GetNameSafe(GetOwner()), CurrentPhaseIndex, *GetPatternTypeName(PatternType), SelectedIndex);

		return false;
	}

	OutAbilityClass = SelectedAbilityClass;

	UE_LOG(LogTemp, Log, TEXT("[BossPhase] Select %s %s"), *GetPatternTypeName(PatternType), *GetNameSafe(SelectedAbilityClass));

	return true;
}

void URSBossPhaseComponent::AdvancePatternCycle()
{
	const int32 CycleLength = GetCycleLength();
	if (CycleLength <= 0)
	{
		return;
	}

	const int32 PreviousCycleIndex = CurrentCycleIndex;
	CurrentCycleIndex = (CurrentCycleIndex + 1) % CycleLength;

	UE_LOG(LogTemp, Log, TEXT("[BossPhase] Advance %d -> %d"), PreviousCycleIndex, CurrentCycleIndex);

	SkipUnusablePatternTurns();
}

void URSBossPhaseComponent::ResetPatternCycle()
{
	CurrentCycleIndex = 0;

	UE_LOG(LogTemp, Log, TEXT("[BossPhase] Reset"));

	SkipUnusablePatternTurns();
}

bool URSBossPhaseComponent::SkipUnusablePatternTurns()
{
	const int32 CycleLength = GetCycleLength();
	if (CycleLength <= 0)
	{
		return false;
	}

	// 한 바퀴만 돌면 모든 차례를 확인하므로 후보가 전부 없을 때 무한히 건너뛰지 않습니다
	for (int32 CheckedTurnCount = 0; CheckedTurnCount < CycleLength; ++CheckedTurnCount)
	{
		ERSBossPatternType CurrentPatternType;
		if (!TryGetCurrentPatternType(CurrentPatternType))
		{
			return false;
		}

		const TArray<TSubclassOf<URSBaseGameplayAbility>>* Candidates = GetPatternCandidates(CurrentPatternType);
		if (Candidates && !Candidates->IsEmpty())
		{
			return true;
		}

		const int32 PreviousCycleIndex = CurrentCycleIndex;
		CurrentCycleIndex = (CurrentCycleIndex + 1) % CycleLength;

		// 정상 수행으로 소비한 차례와 구분할 수 있도록 Advance가 아닌 Skip으로 남깁니다
		UE_LOG(LogTemp, Warning, TEXT("[BossPhase] Skip %d -> %d (%s 후보 없음)"), PreviousCycleIndex, CurrentCycleIndex, *GetPatternTypeName(CurrentPatternType));
	}

	UE_LOG(LogTemp, Warning, TEXT("[BossPhase] %s의 %d번 페이즈는 모든 차례에 후보가 없어 패턴을 실행할 수 없습니다"), *GetNameSafe(GetOwner()), CurrentPhaseIndex);

	return false;
}

int32 URSBossPhaseComponent::GetCycleLength() const
{
	const FRSBossPhaseDefinition* CurrentPhase = GetCurrentPhase();

	return CurrentPhase ? CurrentPhase->CycleSequence.Num() : 0;
}

#pragma endregion

#pragma region Phase

int32 URSBossPhaseComponent::GetPhaseCount() const
{
	return PhaseData ? PhaseData->Phases.Num() : 0;
}

bool URSBossPhaseComponent::TryGetPendingMainGimmick(TSubclassOf<URSBaseGameplayAbility>& OutAbilityClass) const
{
	if (!bPhaseTransitionPending)
	{
		return false;
	}

	const FRSBossPhaseDefinition* CurrentPhase = GetCurrentPhase();
	if (!CurrentPhase || !CurrentPhase->MainGimmickAbility)
	{
		return false;
	}

	OutAbilityClass = CurrentPhase->MainGimmickAbility;

	return true;
}

void URSBossPhaseComponent::AdvanceToNextPhase()
{
	// 마지막 페이즈에는 다음 페이즈가 없으므로 진행 요청을 무시합니다
	if (CurrentPhaseIndex + 1 >= GetPhaseCount())
	{
		UE_LOG(LogTemp, Warning, TEXT("[BossPhase] %s의 %d번 페이즈 다음이 없어 페이즈 전환을 건너뜁니다"), *GetNameSafe(GetOwner()), CurrentPhaseIndex);

		return;
	}

	const int32 PreviousPhaseIndex = CurrentPhaseIndex;
	++CurrentPhaseIndex;

	// 새 페이즈는 사이클을 처음 차례부터 시작하고 이전 페이즈의 기믹 대기를 남기지 않습니다
	bPhaseTransitionPending = false;
	CurrentCycleIndex = 0;

	UE_LOG(LogTemp, Log, TEXT("[BossPhase] Phase %d -> %d"), PreviousPhaseIndex, CurrentPhaseIndex);

	// 새 페이즈의 첫 차례에 후보가 없을 수 있습니다
	SkipUnusablePatternTurns();

	// 새 페이즈의 트리거가 이미 지난 체력일 수 있으므로 전환 직후 한 번 평가합니다
	if (ObservedHealthComponent)
	{
		EvaluateHealthTrigger(ObservedHealthComponent->GetHealth(), ObservedHealthComponent->GetMaxHealth());
	}
}

#pragma endregion

#if WITH_DEV_AUTOMATION_TESTS
void URSBossPhaseComponent::SetPhasesForTest(const TArray<FRSBossPhaseDefinition>& InPhases)
{
	URSBossPhaseData* TestPhaseData = NewObject<URSBossPhaseData>(this);
	TestPhaseData->Phases = InPhases;

	PhaseData = TestPhaseData;
	CurrentPhaseIndex = 0;
	CurrentCycleIndex = 0;
	bPhaseTransitionPending = false;
}

void URSBossPhaseComponent::EvaluateHealthTriggerForTest(float Health, float MaxHealth)
{
	EvaluateHealthTrigger(Health, MaxHealth);
}
#endif

const FRSBossPhaseDefinition* URSBossPhaseComponent::GetCurrentPhase() const
{
	if (!PhaseData || !PhaseData->Phases.IsValidIndex(CurrentPhaseIndex))
	{
		return nullptr;
	}

	return &PhaseData->Phases[CurrentPhaseIndex];
}

const TArray<TSubclassOf<URSBaseGameplayAbility>>* URSBossPhaseComponent::GetPatternCandidates(ERSBossPatternType PatternType) const
{
	const FRSBossPhaseDefinition* CurrentPhase = GetCurrentPhase();
	if (!CurrentPhase)
	{
		return nullptr;
	}

	return PatternType == ERSBossPatternType::Special ? &CurrentPhase->SpecialPatterns : &CurrentPhase->BasicPatterns;
}

URSHealthComponent* URSBossPhaseComponent::FindHealthComponent() const
{
	const AActor* Owner = GetOwner();

	return Owner ? Owner->FindComponentByClass<URSHealthComponent>() : nullptr;
}

void URSBossPhaseComponent::HandleHealthChanged(URSHealthComponent* HealthComponent, float OldValue, float NewValue)
{
	if (!HealthComponent)
	{
		return;
	}

	EvaluateHealthTrigger(NewValue, HealthComponent->GetMaxHealth());
}

void URSBossPhaseComponent::EvaluateHealthTrigger(float Health, float MaxHealth)
{
	if (bPhaseTransitionPending)
	{
		return;
	}

	const FRSBossPhaseDefinition* CurrentPhase = GetCurrentPhase();
	if (!CurrentPhase)
	{
		return;
	}

	// 배열의 마지막 페이즈는 넘어갈 곳이 없으므로 체력 기준을 평가하지 않고 사망 경로만 따릅니다
	if (!HasNextPhase())
	{
		return;
	}

	if (MaxHealth <= 0.0f)
	{
		return;
	}

	// 한 번에 기준 지점을 지나칠 만큼 큰 피해를 받아도 누락되지 않도록 이하 비교를 사용합니다
	const float TriggerHealth = MaxHealth * CurrentPhase->HealthTriggerRatio;
	if (Health > TriggerHealth)
	{
		return;
	}

	bPhaseTransitionPending = true;

	UE_LOG(LogTemp, Log, TEXT("[BossPhase] Transition pending phase %d at %.1f / %.1f (trigger %.1f)"), CurrentPhaseIndex, Health, MaxHealth, TriggerHealth);
}

bool URSBossPhaseComponent::HasNextPhase() const
{
	return CurrentPhaseIndex + 1 < GetPhaseCount();
}
