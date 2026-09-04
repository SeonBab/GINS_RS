// Fill out your copyright notice in the Description page of Project Settings.


#include "RSAbilityDefinition.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#if WITH_EDITOR
EDataValidationResult URSAbilityDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult ValidationResult = Super::IsDataValid(Context);

	// 표시 정보가 비어 있으면 슬롯에 이름이나 아이콘이 없는 채로 노출되므로 애셋 단계에서 잡습니다
	if (DisplayName.IsEmpty())
	{
		Context.AddError(FText::FromString(TEXT("Ability Definition에는 표시할 이름이 필요합니다")));
		ValidationResult = EDataValidationResult::Invalid;
	}

	if (Icon.IsNull())
	{
		Context.AddError(FText::FromString(TEXT("Ability Definition에는 표시할 아이콘이 필요합니다")));
		ValidationResult = EDataValidationResult::Invalid;
	}

	return ValidationResult;
}
#endif
