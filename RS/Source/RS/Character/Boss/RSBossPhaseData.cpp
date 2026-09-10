// Fill out your copyright notice in the Description page of Project Settings.


#include "RSBossPhaseData.h"

// TSubclassOf의 유효성 검사가 대상 클래스의 완전한 정의를 요구합니다
#include "Abilities/RSBaseGameplayAbility.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#if WITH_EDITOR
namespace
{
	/** 사이클에서 사용하는 종류의 후보 목록이 실제로 쓸 수 있는 상태인지 검사합니다 */
	bool ValidatePatternCandidates(const TArray<TSubclassOf<URSBaseGameplayAbility>>& Candidates, int32 PhaseIndex, const TCHAR* PatternTypeName, FDataValidationContext& Context)
	{
		bool bIsValid = true;

		// 사이클에 있는 차례인데 후보가 없으면 런타임에 그 차례를 실행할 방법이 없습니다
		if (Candidates.IsEmpty())
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("%d번 페이즈의 CycleSequence는 %s를 사용하지만 후보 목록이 비어 있습니다"), PhaseIndex, PatternTypeName)));

			return false;
		}

		for (int32 CandidateIndex = 0; CandidateIndex < Candidates.Num(); ++CandidateIndex)
		{
			if (!Candidates[CandidateIndex])
			{
				Context.AddError(FText::FromString(FString::Printf(TEXT("%d번 페이즈의 %s 후보 %d번이 비어 있습니다"), PhaseIndex, PatternTypeName, CandidateIndex)));
				bIsValid = false;
			}
		}

		return bIsValid;
	}
}

EDataValidationResult URSBossPhaseData::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult ValidationResult = Super::IsDataValid(Context);

	if (Phases.IsEmpty())
	{
		Context.AddError(FText::FromString(TEXT("페이즈가 하나도 없어 보스가 어떤 패턴도 사용할 수 없습니다")));

		return EDataValidationResult::Invalid;
	}

	for (int32 PhaseIndex = 0; PhaseIndex < Phases.Num(); ++PhaseIndex)
	{
		const FRSBossPhaseDefinition& Phase = Phases[PhaseIndex];

		if (Phase.CycleSequence.IsEmpty())
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("%d번 페이즈의 CycleSequence가 비어 있어 현재 차례를 결정할 수 없습니다"), PhaseIndex)));
			ValidationResult = EDataValidationResult::Invalid;

			continue;
		}

		// 사이클에 등장하는 종류만 검사합니다. 쓰지 않는 종류의 후보가 비어 있는 것은 정상입니다
		if (Phase.CycleSequence.Contains(ERSBossPatternType::Basic) && !ValidatePatternCandidates(Phase.BasicPatterns, PhaseIndex, TEXT("Basic"), Context))
		{
			ValidationResult = EDataValidationResult::Invalid;
		}

		if (Phase.CycleSequence.Contains(ERSBossPatternType::Special) && !ValidatePatternCandidates(Phase.SpecialPatterns, PhaseIndex, TEXT("Special"), Context))
		{
			ValidationResult = EDataValidationResult::Invalid;
		}

		// 페이즈는 배열 순서대로 진행하므로 마지막 원소에는 넘어갈 다음 페이즈가 없습니다
		// 마지막 페이즈에 남은 기준값은 뒤에 페이즈를 추가하면 그대로 쓰이므로 문제로 보지 않습니다
		const bool bIsLastPhase = PhaseIndex + 1 >= Phases.Num();

		// 다음 페이즈가 있는데 기준이 0이면 체력이 0이 되어야 넘어가므로 사실상 전환되지 않습니다
		if (!bIsLastPhase && Phase.HealthTriggerRatio <= 0.0f)
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("%d번 페이즈의 HealthTriggerRatio가 0이라 다음 페이즈로 넘어가지 않습니다"), PhaseIndex)));
			ValidationResult = EDataValidationResult::Invalid;
		}

		// 체력은 줄어들기만 하므로 뒤 페이즈의 기준이 앞 페이즈보다 낮아야 순서대로 진행됩니다
		// 지금 쓰이지 않는 마지막 페이즈의 값도 함께 검사해 페이즈를 추가한 순간 어긋나 있지 않게 합니다
		if (PhaseIndex > 0 && Phase.HealthTriggerRatio > 0.0f && Phase.HealthTriggerRatio >= Phases[PhaseIndex - 1].HealthTriggerRatio)
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("%d번 페이즈의 HealthTriggerRatio가 앞 페이즈보다 낮지 않아 순서대로 진행되지 않습니다"), PhaseIndex)));
			ValidationResult = EDataValidationResult::Invalid;
		}
	}

	return ValidationResult;
}
#endif
