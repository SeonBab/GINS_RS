// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#if WITH_TESTS

#include "CoreMinimal.h"
#include "Abilities/Player/RSGameplayAbility_DamageImmunityFeedback.h"
#include "GameplayEffect.h"
#include "RSDamageImmunityFeedbackTestTypes.generated.h"

/**
 * 공용 쿨다운 GameplayEffect와 같은 계약을 가지는 자동화 테스트 전용 GameplayEffect입니다
 * 실제 쿨다운은 Blueprint 에셋이므로 콘텐츠에 의존하지 않고 같은 SetByCaller 지속 시간 계약만 재현합니다
 */
UCLASS()
class URSDamageImmunityFeedbackTestCooldownEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	URSDamageImmunityFeedbackTestCooldownEffect();
};

/**
 * 연출 어빌리티를 ASC에 부여하기 위한 자동화 테스트 전용 구체 클래스입니다
 * 실제 사용은 에셋과 쿨다운 값을 지정한 Blueprint 하위 클래스이므로 원본은 Abstract로 남습니다
 * UHT는 WITH_DEV_AUTOMATION_TESTS를 해석하지 못해 안쪽 선언을 건너뛰므로 WITH_TESTS로 감쌉니다
 */
UCLASS()
class URSDamageImmunityFeedbackTestAbility : public URSGameplayAbility_DamageImmunityFeedback
{
	GENERATED_BODY()

public:
	URSDamageImmunityFeedbackTestAbility();
};

#endif // WITH_TESTS
