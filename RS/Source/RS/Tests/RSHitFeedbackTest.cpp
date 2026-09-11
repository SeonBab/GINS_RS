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

	Feedback.CameraShake = UCameraShakeBase::StaticClass();

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
	TestEqual(TEXT("Default shake scale is unscaled"), DefaultFeedback.CameraShakeScale, 1.0f);
	TestEqual(TEXT("Default hit stop is off"), DefaultFeedback.HitStopDuration, 0.0f);

	return true;
}

#endif
