// Fill out your copyright notice in the Description page of Project Settings.

#if WITH_TESTS

#include "RSAbilityOnSpawnTestTypes.h"

URSAbilityOnSpawnTestAbility::URSAbilityOnSpawnTestAbility()
{
	// 실제 상시 어빌리티가 Class Defaults에서 지정할 값을 테스트에서는 생성자로 대신 채웁니다
	ActivationPolicy = ERSAbilityActivationPolicy::OnSpawn;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

URSAbilityOnSpawnTestManualAbility::URSAbilityOnSpawnTestManualAbility()
{
	ActivationPolicy = ERSAbilityActivationPolicy::None;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

#endif // WITH_TESTS
