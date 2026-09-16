#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Components/ProgressBar.h"
#include "RSBossHealthBarWidgetUtilities.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSBossHealthBarStrengthTest, "RS.UI.BossHealthBar.ShakeStrength", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSBossHealthBarLayerBrushesTest, "RS.UI.BossHealthBar.LayerBrushes", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSBossHealthBarDelayedDrainTest, "RS.UI.BossHealthBar.DelayedDrain", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSBossHealthBarDelayedLayerPercentTest, "RS.UI.BossHealthBar.DelayedLayerPercent", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSBossHealthBarLayerDisplayTest, "RS.UI.BossHealthBar.LayerDisplay", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSBossHealthBarStrengthTest::RunTest(const FString& Parameters)
{
	constexpr float SampleTime = 0.0625f;
	const FVector2D ZeroStrength = RSBossHealthBar::CalculateTranslation(0.0f, SampleTime, 8.0f, 2.0f);
	const FVector2D HalfStrength = RSBossHealthBar::CalculateTranslation(0.5f, SampleTime, 8.0f, 2.0f);
	const FVector2D FullStrength = RSBossHealthBar::CalculateTranslation(1.0f, SampleTime, 8.0f, 2.0f);

	TestEqual(TEXT("Strength 0 keeps X at zero"), ZeroStrength.X, 0.0);
	TestEqual(TEXT("Strength 0 keeps Y at zero"), ZeroStrength.Y, 0.0);
	TestTrue(TEXT("Strength 0.5 produces half X amplitude"), FMath::IsNearlyEqual(HalfStrength.X, FullStrength.X * 0.5f));
	TestTrue(TEXT("Strength 0.5 produces half Y amplitude"), FMath::IsNearlyEqual(HalfStrength.Y, FullStrength.Y * 0.5f));
	TestTrue(TEXT("X remains the primary axis"), FMath::Abs(FullStrength.X) > FMath::Abs(FullStrength.Y));

	const FVector2D ClampedHighStrength = RSBossHealthBar::CalculateTranslation(2.0f, SampleTime, 8.0f, 2.0f);
	TestTrue(TEXT("Strength above one clamps to full X amplitude"), FMath::IsNearlyEqual(ClampedHighStrength.X, FullStrength.X));
	TestTrue(TEXT("Strength above one clamps to full Y amplitude"), FMath::IsNearlyEqual(ClampedHighStrength.Y, FullStrength.Y));
	TestEqual(TEXT("Squared envelope reaches exact zero at the end"), RSBossHealthBar::CalculateTranslation(1.0f, 1.0f, 8.0f, 2.0f), FVector2D::ZeroVector);

	return true;
}

bool FRSBossHealthBarLayerBrushesTest::RunTest(const FString& Parameters)
{
	UProgressBar* ProgressBar = NewObject<UProgressBar>();
	const FLinearColor Red(0.85f, 0.08f, 0.08f, 1.0f);
	const FLinearColor Orange(0.91f, 0.32f, 0.05f, 1.0f);
	const FLinearColor Yellow(0.85f, 0.65f, 0.03f, 1.0f);
	const FLinearColor Empty(0.04f, 0.04f, 0.04f, 1.0f);

	RSBossHealthBar::ApplyProgressBarLayerColors(*ProgressBar, Red, Orange);
	TestEqual(TEXT("One ProgressBar fill shows current red layer"), ProgressBar->GetFillColorAndOpacity(), Red);
	TestEqual(TEXT("One ProgressBar background shows next orange layer"), ProgressBar->GetWidgetStyle().BackgroundImage.TintColor.GetSpecifiedColor(), Orange);

	RSBossHealthBar::ApplyProgressBarLayerColors(*ProgressBar, Orange, Yellow);
	TestEqual(TEXT("The same ProgressBar fill advances to orange"), ProgressBar->GetFillColorAndOpacity(), Orange);
	TestEqual(TEXT("The same ProgressBar background advances to yellow"), ProgressBar->GetWidgetStyle().BackgroundImage.TintColor.GetSpecifiedColor(), Yellow);

	RSBossHealthBar::ApplyProgressBarLayerColors(*ProgressBar, Red, Empty);
	TestEqual(TEXT("Last layer background becomes empty color"), ProgressBar->GetWidgetStyle().BackgroundImage.TintColor.GetSpecifiedColor(), Empty);

	return true;
}

bool FRSBossHealthBarDelayedDrainTest::RunTest(const FString& Parameters)
{
	constexpr float DeltaTime = 1.0f / 60.0f;
	constexpr float InterpSpeed = 6.0f;
	constexpr float SnapThreshold = 0.002f;

	const float LargeGapResult = RSBossHealthBar::AdvanceLayerScaled(1.0f, 0.0f, DeltaTime, InterpSpeed, SnapThreshold);
	const float SmallGapResult = RSBossHealthBar::AdvanceLayerScaled(1.0f, 0.9f, DeltaTime, InterpSpeed, SnapThreshold);
	TestTrue(TEXT("A large gap drains faster than a small gap in one frame"), (1.0f - LargeGapResult) > (1.0f - SmallGapResult));
	TestTrue(TEXT("A small gap still drains instead of standing still"), SmallGapResult < 1.0f);
	TestTrue(TEXT("Draining never passes the real health"), LargeGapResult >= 0.0f);

	// 드레인 도중 새 피해로 목표가 내려가면 그 프레임부터 속도가 다시 커져야 합니다
	const float AfterFirstFrame = RSBossHealthBar::AdvanceLayerScaled(1.0f, 0.7f, DeltaTime, InterpSpeed, SnapThreshold);
	const float KeptTargetStep = AfterFirstFrame - RSBossHealthBar::AdvanceLayerScaled(AfterFirstFrame, 0.7f, DeltaTime, InterpSpeed, SnapThreshold);
	const float LoweredTargetStep = AfterFirstFrame - RSBossHealthBar::AdvanceLayerScaled(AfterFirstFrame, 0.2f, DeltaTime, InterpSpeed, SnapThreshold);
	TestTrue(TEXT("A new hit during the drain increases the drain speed"), LoweredTargetStep > KeptTargetStep);

	TestEqual(TEXT("A rising target snaps without a trailing bar"), RSBossHealthBar::AdvanceLayerScaled(0.3f, 0.8f, DeltaTime, InterpSpeed, SnapThreshold), 0.8f);
	TestEqual(TEXT("A gap under the snap threshold reaches the target exactly"), RSBossHealthBar::AdvanceLayerScaled(0.5f, 0.4999f, DeltaTime, InterpSpeed, SnapThreshold), 0.4999f);
	TestEqual(TEXT("A long frame stops at the target instead of overshooting"), RSBossHealthBar::AdvanceLayerScaled(1.0f, 0.0f, 10.0f, InterpSpeed, SnapThreshold), 0.0f);

	return true;
}

bool FRSBossHealthBarDelayedLayerPercentTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("A trailing value inside the active layer shows its own ratio"), RSBossHealthBar::CalculateDelayedLayerPercent(2.7f, 3), 0.7f, UE_KINDA_SMALL_NUMBER);
	TestEqual(TEXT("A trailing value above the active layer fills the whole bar"), RSBossHealthBar::CalculateDelayedLayerPercent(2.7f, 2), 1.0f);
	TestEqual(TEXT("A trailing value caught up at the layer boundary empties the bar"), RSBossHealthBar::CalculateDelayedLayerPercent(2.0f, 3), 0.0f);
	TestEqual(TEXT("The last layer keeps draining after health reaches zero"), RSBossHealthBar::CalculateDelayedLayerPercent(0.5f, 0), 0.5f, UE_KINDA_SMALL_NUMBER);
	TestEqual(TEXT("A finished drain at zero health leaves the bar empty"), RSBossHealthBar::CalculateDelayedLayerPercent(0.0f, 0), 0.0f);

	return true;
}

bool FRSBossHealthBarLayerDisplayTest::RunTest(const FString& Parameters)
{
	const FRSBossHealthLayerDisplay InsideLayer = RSBossHealthBar::CalculateLayerDisplay(2.7f, 4);
	TestEqual(TEXT("A value inside a layer keeps that layer remaining"), InsideLayer.RemainingLayerCount, 3);
	TestEqual(TEXT("A value inside a layer shows the part above the boundary"), InsideLayer.LayerPercent, 0.7f, UE_KINDA_SMALL_NUMBER);

	// 애니메이션이 경계에 정확히 닿는 순간에는 아래 레이어가 아니라 방금 비운 레이어가 가득 찬 상태로 보여야 합니다
	const FRSBossHealthLayerDisplay OnBoundary = RSBossHealthBar::CalculateLayerDisplay(2.0f, 4);
	TestEqual(TEXT("An exact boundary still belongs to the layer above it"), OnBoundary.RemainingLayerCount, 2);
	TestEqual(TEXT("An exact boundary fills that layer"), OnBoundary.LayerPercent, 1.0f);

	const FRSBossHealthLayerDisplay FullHealth = RSBossHealthBar::CalculateLayerDisplay(4.0f, 4);
	TestEqual(TEXT("Full health keeps every layer"), FullHealth.RemainingLayerCount, 4);
	TestEqual(TEXT("Full health fills the active layer"), FullHealth.LayerPercent, 1.0f);

	const FRSBossHealthLayerDisplay NoHealth = RSBossHealthBar::CalculateLayerDisplay(0.0f, 4);
	TestEqual(TEXT("Zero health leaves no layer"), NoHealth.RemainingLayerCount, 0);
	TestEqual(TEXT("Zero health empties the bar"), NoHealth.LayerPercent, 0.0f);

	const FRSBossHealthLayerDisplay AboveRange = RSBossHealthBar::CalculateLayerDisplay(5.0f, 4);
	TestEqual(TEXT("A value above the layer count clamps to the last layer"), AboveRange.RemainingLayerCount, 4);
	TestEqual(TEXT("A value above the layer count clamps to a full bar"), AboveRange.LayerPercent, 1.0f);

	const FRSBossHealthLayerDisplay InvalidLayerCount = RSBossHealthBar::CalculateLayerDisplay(0.5f, 0);
	TestEqual(TEXT("An unset layer count behaves as a single layer"), InvalidLayerCount.RemainingLayerCount, 1);
	TestEqual(TEXT("An unset layer count still shows its ratio"), InvalidLayerCount.LayerPercent, 0.5f, UE_KINDA_SMALL_NUMBER);

	return true;
}

#endif
