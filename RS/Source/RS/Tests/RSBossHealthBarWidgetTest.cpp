#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Components/ProgressBar.h"
#include "RSBossHealthBarWidgetUtilities.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSBossHealthBarShakeStrengthTest, "RS.UI.BossHealthBar.ShakeStrength", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSBossHealthBarLayerBrushesTest, "RS.UI.BossHealthBar.LayerBrushes", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSBossHealthBarShakeStrengthTest::RunTest(const FString& Parameters)
{
	constexpr float SampleTime = 0.0625f;
	const FVector2D ZeroStrength = RSBossHealthBarShake::CalculateTranslation(0.0f, SampleTime, 8.0f, 2.0f);
	const FVector2D HalfStrength = RSBossHealthBarShake::CalculateTranslation(0.5f, SampleTime, 8.0f, 2.0f);
	const FVector2D FullStrength = RSBossHealthBarShake::CalculateTranslation(1.0f, SampleTime, 8.0f, 2.0f);

	TestEqual(TEXT("Strength 0 keeps X at zero"), ZeroStrength.X, 0.0);
	TestEqual(TEXT("Strength 0 keeps Y at zero"), ZeroStrength.Y, 0.0);
	TestTrue(TEXT("Strength 0.5 produces half X amplitude"), FMath::IsNearlyEqual(HalfStrength.X, FullStrength.X * 0.5f));
	TestTrue(TEXT("Strength 0.5 produces half Y amplitude"), FMath::IsNearlyEqual(HalfStrength.Y, FullStrength.Y * 0.5f));
	TestTrue(TEXT("X remains the primary axis"), FMath::Abs(FullStrength.X) > FMath::Abs(FullStrength.Y));

	const FVector2D ClampedHighStrength = RSBossHealthBarShake::CalculateTranslation(2.0f, SampleTime, 8.0f, 2.0f);
	TestTrue(TEXT("Strength above one clamps to full X amplitude"), FMath::IsNearlyEqual(ClampedHighStrength.X, FullStrength.X));
	TestTrue(TEXT("Strength above one clamps to full Y amplitude"), FMath::IsNearlyEqual(ClampedHighStrength.Y, FullStrength.Y));
	TestEqual(TEXT("Squared envelope reaches exact zero at the end"), RSBossHealthBarShake::CalculateTranslation(1.0f, 1.0f, 8.0f, 2.0f), FVector2D::ZeroVector);

	return true;
}

bool FRSBossHealthBarLayerBrushesTest::RunTest(const FString& Parameters)
{
	UProgressBar* ProgressBar = NewObject<UProgressBar>();
	const FLinearColor Red(0.85f, 0.08f, 0.08f, 1.0f);
	const FLinearColor Orange(0.91f, 0.32f, 0.05f, 1.0f);
	const FLinearColor Yellow(0.85f, 0.65f, 0.03f, 1.0f);
	const FLinearColor Empty(0.04f, 0.04f, 0.04f, 1.0f);

	RSBossHealthBarShake::ApplyProgressBarLayerColors(*ProgressBar, Red, Orange);
	TestEqual(TEXT("One ProgressBar fill shows current red layer"), ProgressBar->GetFillColorAndOpacity(), Red);
	TestEqual(TEXT("One ProgressBar background shows next orange layer"), ProgressBar->GetWidgetStyle().BackgroundImage.TintColor.GetSpecifiedColor(), Orange);

	RSBossHealthBarShake::ApplyProgressBarLayerColors(*ProgressBar, Orange, Yellow);
	TestEqual(TEXT("The same ProgressBar fill advances to orange"), ProgressBar->GetFillColorAndOpacity(), Orange);
	TestEqual(TEXT("The same ProgressBar background advances to yellow"), ProgressBar->GetWidgetStyle().BackgroundImage.TintColor.GetSpecifiedColor(), Yellow);

	RSBossHealthBarShake::ApplyProgressBarLayerColors(*ProgressBar, Red, Empty);
	TestEqual(TEXT("Last layer background becomes empty color"), ProgressBar->GetWidgetStyle().BackgroundImage.TintColor.GetSpecifiedColor(), Empty);

	return true;
}

#endif
