#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Camera/CameraShakeBase.h"
#include "Combat/RSCombatFunctionLibrary.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSHitFeedbackTest, "RS.Combat.HitFeedback", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSHitFeedbackTest::RunTest(const FString& Parameters)
{
	FRSHitFeedbackDefinition Feedback;

	// 셰이크를 지정하지 않은 공격은 어느 시점에도 흔들지 않아야 합니다
	Feedback.CameraShakeTiming = ERSCameraShakeTiming::OnHit;
	TestFalse(TEXT("No shake asset never shakes on hit"), URSCombatFunctionLibrary::ShouldPlayCameraShake(Feedback, true));
	Feedback.CameraShakeTiming = ERSCameraShakeTiming::OnHitCheck;
	TestFalse(TEXT("No shake asset never shakes on hit check"), URSCombatFunctionLibrary::ShouldPlayCameraShake(Feedback, true));

	Feedback.CameraShake.ShakeClass = UCameraShakeBase::StaticClass();

	// 플레이어 기본 공격은 맞췄을 때만 흔듭니다
	Feedback.CameraShakeTiming = ERSCameraShakeTiming::OnHit;
	TestTrue(TEXT("OnHit shakes when the attack landed"), URSCombatFunctionLibrary::ShouldPlayCameraShake(Feedback, true));
	TestFalse(TEXT("OnHit stays quiet when the attack missed"), URSCombatFunctionLibrary::ShouldPlayCameraShake(Feedback, false));

	// 보스 패턴은 빗나가도 판정 순간마다 흔듭니다
	Feedback.CameraShakeTiming = ERSCameraShakeTiming::OnHitCheck;
	TestTrue(TEXT("OnHitCheck shakes when the attack landed"), URSCombatFunctionLibrary::ShouldPlayCameraShake(Feedback, true));
	TestTrue(TEXT("OnHitCheck shakes even when the attack missed"), URSCombatFunctionLibrary::ShouldPlayCameraShake(Feedback, false));

	// 기본값은 플레이어 공격 쪽이라 설정하지 않은 공격이 빈 판정마다 화면을 흔들지 않습니다
	const FRSHitFeedbackDefinition DefaultFeedback;
	TestEqual(TEXT("Default shake timing is OnHit"), DefaultFeedback.CameraShakeTiming, ERSCameraShakeTiming::OnHit);
	TestEqual(TEXT("Default shake scale is unscaled"), DefaultFeedback.CameraShake.Scale, 1.0f);
	TestEqual(TEXT("Default hit stop is off"), DefaultFeedback.HitStopDuration, 0.0f);
	TestEqual(TEXT("Default impact pull is half of the target radius"), DefaultFeedback.ImpactNiagaraPullRatio, 0.5f);

	constexpr float Tolerance = 0.01f;
	constexpr float AttackerCapsuleHalfHeight = 90.0f;
	constexpr float TargetCapsuleRadius = 200.0f;
	constexpr float PullRatio = 0.5f;

	// 연출 높이는 대상 캡슐 바닥에서 공격자의 반높이만큼이며, 대상이 커져도 따라 올라가지 않습니다
	// 수평으로는 대상 중심에서 공격자 쪽으로 반지름의 절반만큼 당깁니다
	const FVector PulledLocation = URSCombatFunctionLibrary::CalculateHitImpactLocation(FVector(0.0f, 0.0f, 0.0f), FVector(300.0f, 0.0f, 0.0f), AttackerCapsuleHalfHeight, TargetCapsuleRadius, PullRatio);
	TestEqual(TEXT("Impact effect is pulled toward the attacker at the attacker capsule half height"), PulledLocation, FVector(200.0f, 0.0f, 90.0f), Tolerance);

	// 서로의 높이 차이는 수평으로 당기는 방향과 거리를 바꾸지 않아야 합니다
	const FVector RaisedAttackerLocation = URSCombatFunctionLibrary::CalculateHitImpactLocation(FVector(0.0f, 0.0f, 500.0f), FVector(300.0f, 0.0f, 0.0f), AttackerCapsuleHalfHeight, TargetCapsuleRadius, PullRatio);
	TestEqual(TEXT("Height difference does not change the horizontal pull"), RaisedAttackerLocation, PulledLocation, Tolerance);

	// 수평으로 겹쳐 서면 당길 방향이 없으므로 대상 중심에 그대로 둡니다
	const FVector OverlappedLocation = URSCombatFunctionLibrary::CalculateHitImpactLocation(FVector(300.0f, 0.0f, 50.0f), FVector(300.0f, 0.0f, 0.0f), AttackerCapsuleHalfHeight, TargetCapsuleRadius, PullRatio);
	TestEqual(TEXT("Overlapping actors keep the effect at the target center"), OverlappedLocation, FVector(300.0f, 0.0f, 90.0f), Tolerance);

	// 당길 거리가 두 액터 사이보다 멀어도 연출이 공격자 뒤로 넘어가지 않아야 합니다
	const FVector ClampedLocation = URSCombatFunctionLibrary::CalculateHitImpactLocation(FVector(60.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, 0.0f), AttackerCapsuleHalfHeight, TargetCapsuleRadius, PullRatio);
	TestEqual(TEXT("Pull distance never passes the attacker"), ClampedLocation, FVector(60.0f, 0.0f, 90.0f), Tolerance);

	// 비율이 0이면 예전처럼 대상 중심에서 재생하며 높이만 달라집니다
	const FVector UnpulledLocation = URSCombatFunctionLibrary::CalculateHitImpactLocation(FVector(0.0f, 0.0f, 0.0f), FVector(300.0f, 0.0f, 0.0f), AttackerCapsuleHalfHeight, TargetCapsuleRadius, 0.0f);
	TestEqual(TEXT("Zero ratio keeps the effect at the target center"), UnpulledLocation, FVector(300.0f, 0.0f, 90.0f), Tolerance);

	return true;
}

#endif
