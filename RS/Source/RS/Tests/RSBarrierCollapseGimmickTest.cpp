// Fill out your copyright notice in the Description page of Project Settings.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "RSGameplayAbility_Barrier_Collapse_Gimmick.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSBarrierCollapseZoneCentersTest, "RS.Ability.Boss.BarrierCollapse.ZoneCenters", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSBarrierCollapseZoneCentersTest::RunTest(const FString& Parameters)
{
	const FVector PatternCenter(100.0f, 200.0f, 300.0f);
	const float DistanceFromCenter = FMath::Sqrt(2.0f) * 100.0f;

	TArray<FVector> ZoneCenters;
	URSGameplayAbility_Barrier_Collapse_Gimmick::CalculateBarrierZoneCenters(PatternCenter, DistanceFromCenter, ZoneCenters);

	TestEqual(TEXT("Four zones are created"), ZoneCenters.Num(), 4);
	if (ZoneCenters.Num() != 4)
	{
		return false;
	}

	// 45도부터 90도 간격이므로 거리가 sqrt(2) * 100이면 각 중심은 두 축에서 100씩 떨어집니다
	TestEqual(TEXT("Zone 0 sits at 45 degrees"), ZoneCenters[0], PatternCenter + FVector(100.0f, 100.0f, 0.0f));
	TestEqual(TEXT("Zone 1 sits at 135 degrees"), ZoneCenters[1], PatternCenter + FVector(-100.0f, 100.0f, 0.0f));
	TestEqual(TEXT("Zone 2 sits at 225 degrees"), ZoneCenters[2], PatternCenter + FVector(-100.0f, -100.0f, 0.0f));
	TestEqual(TEXT("Zone 3 sits at 315 degrees"), ZoneCenters[3], PatternCenter + FVector(100.0f, -100.0f, 0.0f));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSBarrierCollapseSafeZoneIndexTest, "RS.Ability.Boss.BarrierCollapse.SafeZoneIndex", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSBarrierCollapseSafeZoneIndexTest::RunTest(const FString& Parameters)
{
	const TArray<FVector> ZoneCenters = { FVector(0.0f, 0.0f, 0.0f), FVector(500.0f, 0.0f, 0.0f) };
	const float SafeRadius = 50.0f;

	TestEqual(TEXT("A location at the first center uses zone 0"), URSGameplayAbility_Barrier_Collapse_Gimmick::FindSafeZoneIndex(ZoneCenters, ZoneCenters[0], SafeRadius), 0);
	TestEqual(TEXT("A location at the second center uses zone 1"), URSGameplayAbility_Barrier_Collapse_Gimmick::FindSafeZoneIndex(ZoneCenters, ZoneCenters[1], SafeRadius), 1);

	// 경계는 안전에 포함하고 판정은 수평 거리만 쓰므로 Z 차이는 결과를 바꾸지 않습니다
	TestEqual(TEXT("The inclusive boundary is safe"), URSGameplayAbility_Barrier_Collapse_Gimmick::FindSafeZoneIndex(ZoneCenters, FVector(30.0f, 40.0f, 1000.0f), SafeRadius), 0);
	TestEqual(TEXT("Just outside the boundary is unsafe"), URSGameplayAbility_Barrier_Collapse_Gimmick::FindSafeZoneIndex(ZoneCenters, FVector(30.1f, 40.0f, 0.0f), SafeRadius), INDEX_NONE);
	TestEqual(TEXT("A location between the zones is unsafe"), URSGameplayAbility_Barrier_Collapse_Gimmick::FindSafeZoneIndex(ZoneCenters, FVector(250.0f, 0.0f, 0.0f), SafeRadius), INDEX_NONE);

	// 남은 방어막이 없으면 어디에 서 있어도 안전할 수 없습니다
	TestEqual(TEXT("An empty field is never safe"), URSGameplayAbility_Barrier_Collapse_Gimmick::FindSafeZoneIndex(TArray<FVector>(), FVector::ZeroVector, SafeRadius), INDEX_NONE);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSBarrierCollapseHitCheckResolutionTest, "RS.Ability.Boss.BarrierCollapse.HitCheckResolution", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSBarrierCollapseHitCheckResolutionTest::RunTest(const FString& Parameters)
{
	const TArray<FVector> ZoneCenters = { FVector(0.0f, 0.0f, 0.0f), FVector(500.0f, 0.0f, 0.0f), FVector(1000.0f, 0.0f, 0.0f) };
	const float SafeRadius = 50.0f;

	TArray<int32> DestroyedZoneIndices;
	TArray<int32> FailedTargetIndices;

	// 안전한 대상 하나는 자기가 밟은 방어막만 파괴하고 실패를 남기지 않습니다
	URSGameplayAbility_Barrier_Collapse_Gimmick::ResolveHitCheck(ZoneCenters, TArray<FVector>({ FVector(500.0f, 0.0f, 0.0f) }), SafeRadius, DestroyedZoneIndices, FailedTargetIndices);
	TestEqual(TEXT("One safe target destroys one zone"), DestroyedZoneIndices, TArray<int32>({ 1 }));
	TestEqual(TEXT("One safe target fails nobody"), FailedTargetIndices.Num(), 0);

	// 같은 방어막에 둘이 들어가도 파괴는 한 번이며, 판정 중에 지우지 않으므로 두 번째 대상이 실패로 바뀌지 않습니다
	URSGameplayAbility_Barrier_Collapse_Gimmick::ResolveHitCheck(ZoneCenters, TArray<FVector>({ FVector(0.0f, 0.0f, 0.0f), FVector(30.0f, 40.0f, 0.0f) }), SafeRadius, DestroyedZoneIndices, FailedTargetIndices);
	TestEqual(TEXT("Two targets in one zone destroy it once"), DestroyedZoneIndices, TArray<int32>({ 0 }));
	TestEqual(TEXT("Neither target in the shared zone fails"), FailedTargetIndices.Num(), 0);

	// 어느 방어막에도 없는 대상은 실패하고 방어막은 그대로 남습니다
	URSGameplayAbility_Barrier_Collapse_Gimmick::ResolveHitCheck(ZoneCenters, TArray<FVector>({ FVector(250.0f, 0.0f, 0.0f) }), SafeRadius, DestroyedZoneIndices, FailedTargetIndices);
	TestEqual(TEXT("A failing target destroys nothing"), DestroyedZoneIndices.Num(), 0);
	TestEqual(TEXT("A failing target is reported"), FailedTargetIndices, TArray<int32>({ 0 }));

	// 섞여 있으면 안전한 쪽의 방어막만 오름차순으로 모으고 실패한 쪽만 보고합니다
	URSGameplayAbility_Barrier_Collapse_Gimmick::ResolveHitCheck(ZoneCenters, TArray<FVector>({ FVector(1000.0f, 0.0f, 0.0f), FVector(250.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, 0.0f) }), SafeRadius, DestroyedZoneIndices, FailedTargetIndices);
	TestEqual(TEXT("Destroyed zones are unique and ascending"), DestroyedZoneIndices, TArray<int32>({ 0, 2 }));
	TestEqual(TEXT("Only the unsafe target is reported"), FailedTargetIndices, TArray<int32>({ 1 }));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSBarrierCollapseDepletionTest, "RS.Ability.Boss.BarrierCollapse.Depletion", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSBarrierCollapseDepletionTest::RunTest(const FString& Parameters)
{
	const float DistanceFromCenter = FMath::Sqrt(2.0f) * 100.0f;
	const float SafeRadius = 50.0f;

	TArray<FVector> RemainingZones;
	URSGameplayAbility_Barrier_Collapse_Gimmick::CalculateBarrierZoneCenters(FVector::ZeroVector, DistanceFromCenter, RemainingZones);

	TArray<int32> DestroyedZoneIndices;
	TArray<int32> FailedTargetIndices;
	const int32 ExpectedRemainingCounts[] = { 3, 2, 1 };

	// 혼자 플레이할 때 남은 방어막 중 하나를 밟는 것을 세 번 반복하면 4 -> 3 -> 2로 줄어들고 마지막 공격에도 갈 곳이 남습니다
	for (int32 AttackIndex = 0; AttackIndex < 3; ++AttackIndex)
	{
		TestTrue(FString::Printf(TEXT("Attack %d still has a barrier to use"), AttackIndex), RemainingZones.Num() > 0);
		if (RemainingZones.Num() == 0)
		{
			return false;
		}

		URSGameplayAbility_Barrier_Collapse_Gimmick::ResolveHitCheck(RemainingZones, TArray<FVector>({ RemainingZones[0] }), SafeRadius, DestroyedZoneIndices, FailedTargetIndices);
		TestEqual(FString::Printf(TEXT("Attack %d has no failure"), AttackIndex), FailedTargetIndices.Num(), 0);
		TestEqual(FString::Printf(TEXT("Attack %d destroys one zone"), AttackIndex), DestroyedZoneIndices, TArray<int32>({ 0 }));

		for (int32 RemoveIndex = DestroyedZoneIndices.Num() - 1; RemoveIndex >= 0; --RemoveIndex)
		{
			RemainingZones.RemoveAt(DestroyedZoneIndices[RemoveIndex]);
		}

		TestEqual(FString::Printf(TEXT("Attack %d leaves the expected barrier count"), AttackIndex), RemainingZones.Num(), ExpectedRemainingCounts[AttackIndex]);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSBarrierCollapseFieldValidationTest, "RS.Ability.Boss.BarrierCollapse.FieldValidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSBarrierCollapseFieldValidationTest::RunTest(const FString& Parameters)
{
	const float DistanceFromCenter = 400.0f;

	// 인접한 두 원의 중심 거리가 DistanceFromCenter * sqrt(2)이므로 SafeRadius가 그 절반에 닿으면 두 방어막이 맞닿습니다
	const float TouchingRadius = DistanceFromCenter / FMath::Sqrt(2.0f);

	TestTrue(TEXT("A field with separated zones is valid"), URSGameplayAbility_Barrier_Collapse_Gimmick::IsBarrierFieldValid(DistanceFromCenter, TouchingRadius - 1.0f, nullptr));
	TestFalse(TEXT("Touching zones are invalid"), URSGameplayAbility_Barrier_Collapse_Gimmick::IsBarrierFieldValid(DistanceFromCenter, TouchingRadius, nullptr));
	TestFalse(TEXT("Overlapping zones are invalid"), URSGameplayAbility_Barrier_Collapse_Gimmick::IsBarrierFieldValid(DistanceFromCenter, TouchingRadius + 1.0f, nullptr));
	TestFalse(TEXT("A zero radius is invalid"), URSGameplayAbility_Barrier_Collapse_Gimmick::IsBarrierFieldValid(DistanceFromCenter, 0.0f, nullptr));
	TestFalse(TEXT("A zero distance is invalid"), URSGameplayAbility_Barrier_Collapse_Gimmick::IsBarrierFieldValid(0.0f, 50.0f, nullptr));

	FString ValidationError;
	TestFalse(TEXT("An invalid field reports an error"), URSGameplayAbility_Barrier_Collapse_Gimmick::IsBarrierFieldValid(DistanceFromCenter, TouchingRadius, &ValidationError));
	TestTrue(TEXT("The reported error is not empty"), !ValidationError.IsEmpty());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSBarrierCollapseAttackBeamTransformTest, "RS.Ability.Boss.BarrierCollapse.AttackBeamTransform", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSBarrierCollapseAttackBeamTransformTest::RunTest(const FString& Parameters)
{
	// 패턴이 시작될 때 보스가 월드 +X를 보고 있었다고 가정합니다
	const FTransform PatternTransform(FRotator::ZeroRotator, FVector(100.0f, 200.0f, 50.0f));

	// 공격 시점에 보스가 플레이어를 향해 월드 +Y로 돌면 Beam도 그 방향을 따라야 합니다
	const FTransform TurnedTransform = URSGameplayAbility_Barrier_Collapse_Gimmick::MakeAttackBeamPatternTransform(PatternTransform, FVector(0.0f, 1.0f, 0.0f));
	TestEqual(TEXT("The beam origin stays at the pattern center"), TurnedTransform.GetLocation(), PatternTransform.GetLocation());
	TestEqual(TEXT("The beam yaw follows the current forward"), TurnedTransform.Rotator().Yaw, 90.0);

	// 방어막과 판정은 수평 평면에만 놓이므로 전방의 Z 성분은 Beam을 기울이지 않습니다
	const FTransform TiltedTransform = URSGameplayAbility_Barrier_Collapse_Gimmick::MakeAttackBeamPatternTransform(PatternTransform, FVector(0.0f, 1.0f, 5.0f));
	TestEqual(TEXT("A vertical forward component does not change the yaw"), TiltedTransform.Rotator().Yaw, 90.0);
	TestEqual(TEXT("A vertical forward component does not pitch the beam"), TiltedTransform.Rotator().Pitch, 0.0);

	// Beam 생성 실패는 패턴을 취소하지 않으므로, 전방을 정규화할 수 없으면 시작 회전을 그대로 유지합니다
	const FTransform FallbackTransform = URSGameplayAbility_Barrier_Collapse_Gimmick::MakeAttackBeamPatternTransform(PatternTransform, FVector::ZeroVector);
	TestEqual(TEXT("A zero forward keeps the starting yaw"), FallbackTransform.Rotator().Yaw, PatternTransform.Rotator().Yaw);
	TestEqual(TEXT("A zero forward keeps the pattern center"), FallbackTransform.GetLocation(), PatternTransform.GetLocation());

	return true;
}

#endif
