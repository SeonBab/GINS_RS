// Fill out your copyright notice in the Description page of Project Settings.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "RSGameplayAbility_Barrier_Memory_Gimmick.h"
#include "RSGameplayAbility_BossGroggy.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSBarrierMemoryColorSequenceTest, "RS.Ability.Boss.BarrierMemory.ColorSequence", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSBarrierMemoryColorSequenceTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Red start step 0"), URSGameplayAbility_Barrier_Memory_Gimmick::GetAlternatingColor(ERSBarrierColor::Red, 0), ERSBarrierColor::Red);
	TestEqual(TEXT("Red start step 1"), URSGameplayAbility_Barrier_Memory_Gimmick::GetAlternatingColor(ERSBarrierColor::Red, 1), ERSBarrierColor::Yellow);
	TestEqual(TEXT("Red start step 2"), URSGameplayAbility_Barrier_Memory_Gimmick::GetAlternatingColor(ERSBarrierColor::Red, 2), ERSBarrierColor::Red);
	TestEqual(TEXT("Yellow start step 0"), URSGameplayAbility_Barrier_Memory_Gimmick::GetAlternatingColor(ERSBarrierColor::Yellow, 0), ERSBarrierColor::Yellow);
	TestEqual(TEXT("Yellow start step 1"), URSGameplayAbility_Barrier_Memory_Gimmick::GetAlternatingColor(ERSBarrierColor::Yellow, 1), ERSBarrierColor::Red);
	TestEqual(TEXT("Yellow start step 2"), URSGameplayAbility_Barrier_Memory_Gimmick::GetAlternatingColor(ERSBarrierColor::Yellow, 2), ERSBarrierColor::Yellow);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSBarrierMemorySafeZoneTest, "RS.Ability.Boss.BarrierMemory.SafeZone", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSBarrierMemorySafeZoneTest::RunTest(const FString& Parameters)
{
	const FVector Center(100.0f, 200.0f, 300.0f);
	TestTrue(TEXT("Center is safe"), URSGameplayAbility_Barrier_Memory_Gimmick::IsLocationInsideSafeZone(Center, Center, 50.0f));
	TestTrue(TEXT("Inclusive boundary is safe"), URSGameplayAbility_Barrier_Memory_Gimmick::IsLocationInsideSafeZone(Center + FVector(30.0f, 40.0f, 1000.0f), Center, 50.0f));
	TestFalse(TEXT("Outside boundary is unsafe"), URSGameplayAbility_Barrier_Memory_Gimmick::IsLocationInsideSafeZone(Center + FVector(30.1f, 40.0f, 0.0f), Center, 50.0f));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSBarrierMemoryBeamTransformTest, "RS.Ability.Boss.BarrierMemory.BeamTransform", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSBarrierMemoryBeamTransformTest::RunTest(const FString& Parameters)
{
	// 보스가 월드 +Y를 볼 때 전방 오프셋이 월드 +Y로 나가야 오프셋의 X가 전방이라는 계약이 지켜집니다
	const FTransform PatternTransform(FRotator(0.0f, 90.0f, 0.0f), FVector(100.0f, 200.0f, 50.0f));
	const FTransform BeamTransform = URSGameplayAbility_Barrier_Memory_Gimmick::CalculateBeamTransform(PatternTransform, FVector(300.0f, 0.0f, 20.0f));

	TestEqual(TEXT("Forward offset follows the pattern yaw"), BeamTransform.GetLocation(), FVector(100.0f, 500.0f, 70.0f));
	TestEqual(TEXT("Beam keeps the pattern yaw"), BeamTransform.Rotator().Yaw, 90.0);

	const FTransform ZeroOffsetTransform = URSGameplayAbility_Barrier_Memory_Gimmick::CalculateBeamTransform(PatternTransform, FVector::ZeroVector);
	TestEqual(TEXT("Zero offset stays at the pattern origin"), ZeroOffsetTransform.GetLocation(), PatternTransform.GetLocation());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSBossGroggyTimingTest, "RS.Ability.Boss.Groggy.Timing", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSBossGroggyTimingTest::RunTest(const FString& Parameters)
{
	int32 LoopCount = 0;
	float EffectiveDuration = 0.0f;
	TestTrue(TEXT("Timing is valid"), URSGameplayAbility_BossGroggy::TryCalculateGroggyTiming(10.0f, 1.0f, 2.0f, 1.0f, LoopCount, EffectiveDuration));
	TestEqual(TEXT("Only complete loops fit"), LoopCount, 4);
	TestEqual(TEXT("Effective duration does not exceed request"), EffectiveDuration, 10.0f);

	TestTrue(TEXT("Remainder does not add a partial loop"), URSGameplayAbility_BossGroggy::TryCalculateGroggyTiming(9.9f, 1.0f, 2.0f, 1.0f, LoopCount, EffectiveDuration));
	TestEqual(TEXT("Remainder loop count"), LoopCount, 3);
	TestEqual(TEXT("Remainder effective duration"), EffectiveDuration, 8.0f);

	TestFalse(TEXT("At least one loop is required"), URSGameplayAbility_BossGroggy::TryCalculateGroggyTiming(3.9f, 1.0f, 2.0f, 1.0f, LoopCount, EffectiveDuration));

	return true;
}

#endif
