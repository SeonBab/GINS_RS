// Fill out your copyright notice in the Description page of Project Settings.

#include "RSBossHealthBarWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/ProgressBar.h"
#include "Components/Widget.h"
#include "INotifyFieldValueChanged.h"
#include "MVVMSubsystem.h"
#include "RSBossStatusViewModel.h"
#include "View/MVVMView.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

namespace RSBossHealthBarShake
{
	const FName BossStatusViewModelName = TEXT("BossStatusViewModel");
	const FName MainHealthProgressBarName = TEXT("ProgressBar");

	FVector2D CalculateTranslation(float Strength, float NormalizedTime, float MaxShakeX, float MaxShakeY)
	{
		const float ClampedStrength = FMath::Clamp(Strength, 0.0f, 1.0f);
		const float ClampedTime = FMath::Clamp(NormalizedTime, 0.0f, 1.0f);
		const float Envelope = FMath::Square(1.0f - ClampedTime);
		const float ScaledEnvelope = ClampedStrength * Envelope;
		const float ShakeX = FMath::Sin(ClampedTime * 8.0f * UE_PI) * MaxShakeX * ScaledEnvelope;
		const float ShakeY = FMath::Sin(ClampedTime * 11.0f * UE_PI) * MaxShakeY * ScaledEnvelope;
		return FVector2D(ShakeX, ShakeY);
	}
}

void URSBossHealthBarWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BindToBossStatusViewModel();
}

void URSBossHealthBarWidget::RequestHealthBarShake(float Strength)
{
	// 연타는 진행 중 위치를 남기지 않고 항상 원점에서 새로 시작합니다
	ResetHealthBarShake();

	const float ClampedStrength = FMath::Clamp(Strength, 0.0f, 1.0f);
	if (ClampedStrength <= 0.0f || ShakeDuration <= 0.0f)
	{
		return;
	}

	CurrentShakeStrength = ClampedStrength;
	bIsShakeActive = true;
}

void URSBossHealthBarWidget::ResetHealthBarShake()
{
	bIsShakeActive = false;
	ShakeElapsed = 0.0f;
	CurrentShakeStrength = 0.0f;
	ApplyShakeTranslation(FVector2D::ZeroVector);
}

void URSBossHealthBarWidget::NativeDestruct()
{
	UnbindFromBossStatusViewModel();
	ResetHealthBarShake();

	Super::NativeDestruct();
}

void URSBossHealthBarWidget::BindToBossStatusViewModel()
{
	UnbindFromBossStatusViewModel();

	UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(this);
	if (!View)
	{
		return;
	}

	const TScriptInterface<INotifyFieldValueChanged> ViewModelInterface = View->GetViewModel(RSBossHealthBarShake::BossStatusViewModelName);
	URSBossStatusViewModel* BossStatusViewModel = Cast<URSBossStatusViewModel>(ViewModelInterface.GetObject());
	if (!BossStatusViewModel)
	{
		return;
	}

	BoundBossStatusViewModel = BossStatusViewModel;
	BossStatusViewModel->OnHealthBarShakeRequested.AddUniqueDynamic(this, &ThisClass::RequestHealthBarShake);
	BossStatusViewModel->OnHealthBarShakeResetRequested.AddUniqueDynamic(this, &ThisClass::ResetHealthBarShake);
	LayerColorChangedHandle = BossStatusViewModel->AddFieldValueChangedDelegate(
		URSBossStatusViewModel::FFieldNotificationClassDescriptor::CurrentLayerColorIndex,
		INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateUObject(this, &ThisClass::HandleLayerVisualChanged));
	RemainingLayerCountChangedHandle = BossStatusViewModel->AddFieldValueChangedDelegate(
		URSBossStatusViewModel::FFieldNotificationClassDescriptor::RemainingLayerCount,
		INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateUObject(this, &ThisClass::HandleLayerVisualChanged));
	ApplyLayerColors();
}

void URSBossHealthBarWidget::UnbindFromBossStatusViewModel()
{
	if (URSBossStatusViewModel* BossStatusViewModel = BoundBossStatusViewModel.Get())
	{
		BossStatusViewModel->OnHealthBarShakeRequested.RemoveDynamic(this, &ThisClass::RequestHealthBarShake);
		BossStatusViewModel->OnHealthBarShakeResetRequested.RemoveDynamic(this, &ThisClass::ResetHealthBarShake);
		if (LayerColorChangedHandle.IsValid())
		{
			BossStatusViewModel->RemoveFieldValueChangedDelegate(URSBossStatusViewModel::FFieldNotificationClassDescriptor::CurrentLayerColorIndex, LayerColorChangedHandle);
		}
		if (RemainingLayerCountChangedHandle.IsValid())
		{
			BossStatusViewModel->RemoveFieldValueChangedDelegate(URSBossStatusViewModel::FFieldNotificationClassDescriptor::RemainingLayerCount, RemainingLayerCountChangedHandle);
		}
	}

	LayerColorChangedHandle.Reset();
	RemainingLayerCountChangedHandle.Reset();
	BoundBossStatusViewModel.Reset();
}

void URSBossHealthBarWidget::HandleLayerVisualChanged(UObject*, UE::FieldNotification::FFieldId)
{
	ApplyLayerColors();
}

void URSBossHealthBarWidget::ApplyLayerColors()
{
	const URSBossStatusViewModel* BossStatusViewModel = BoundBossStatusViewModel.Get();
	UProgressBar* MainHealthProgressBar = WidgetTree ? Cast<UProgressBar>(WidgetTree->FindWidget(RSBossHealthBarShake::MainHealthProgressBarName)) : nullptr;
	if (!BossStatusViewModel || !MainHealthProgressBar)
	{
		return;
	}

	const int32 CurrentColorIndex = BossStatusViewModel->GetCurrentLayerColorIndex();
	MainHealthProgressBar->SetFillColorAndOpacity(GetLayerColor(CurrentColorIndex));

	FProgressBarStyle LayerStyle = MainHealthProgressBar->GetWidgetStyle();
	const FLinearColor BackgroundColor = BossStatusViewModel->GetRemainingLayerCount() >= 2 ? GetLayerColor(CurrentColorIndex + 1) : EmptyLayerColor;
	LayerStyle.BackgroundImage.TintColor = FSlateColor(BackgroundColor);
	MainHealthProgressBar->SetWidgetStyle(LayerStyle);
}

FLinearColor URSBossHealthBarWidget::GetLayerColor(int32 ColorIndex) const
{
	switch ((ColorIndex % 4 + 4) % 4)
	{
	case 0:
		return RedLayerColor;
	case 1:
		return OrangeLayerColor;
	case 2:
		return YellowLayerColor;
	default:
		return GreenLayerColor;
	}
}

void URSBossHealthBarWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bIsShakeActive)
	{
		return;
	}

	ShakeElapsed += InDeltaTime;
	const float NormalizedTime = FMath::Clamp(ShakeElapsed / ShakeDuration, 0.0f, 1.0f);
	if (NormalizedTime >= 1.0f)
	{
		ResetHealthBarShake();
		return;
	}

	// 초반 타격감을 살리고 끝에서 부드럽게 0으로 수렴하도록 제곱 감쇠를 사용합니다
	ApplyShakeTranslation(RSBossHealthBarShake::CalculateTranslation(CurrentShakeStrength, NormalizedTime, MaxShakeX, MaxShakeY));
}

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSBossHealthBarShakeStrengthTest, "RS.UI.BossHealthBar.ShakeStrength", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

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

#endif

void URSBossHealthBarWidget::ApplyShakeTranslation(const FVector2D& Translation)
{
	if (WidgetTree && WidgetTree->RootWidget)
	{
		WidgetTree->RootWidget->SetRenderTranslation(Translation);
	}
}
