// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "RSAttributeSet.generated.h"

class URSAbilitySystemComponent;

/** GAS Attribute의 표준 접근자 함수를 한 번에 생성합니다 */
#define RS_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * RS 프로젝트에서 사용하는 모든 AttributeSet의 기반 클래스입니다
 * 공통 접근자와 RS 전용 ASC 접근 기능을 제공합니다
 */
UCLASS(Abstract)
class RS_API URSAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	/** 이 AttributeSet을 소유한 RS AbilitySystemComponent를 반환합니다 */
	URSAbilitySystemComponent* GetRSAbilitySystemComponent() const;
};
