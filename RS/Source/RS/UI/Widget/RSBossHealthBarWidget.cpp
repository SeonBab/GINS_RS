// Fill out your copyright notice in the Description page of Project Settings.

#include "RSBossHealthBarWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/ProgressBar.h"
#include "Components/Widget.h"
#include "INotifyFieldValueChanged.h"
#include "MVVMSubsystem.h"
#include "RSBossStatusViewModel.h"
#include "RSBossHealthBarWidgetUtilities.h"
#include "View/MVVMView.h"

namespace RSBossHealthBar
{
	const FName BossStatusViewModelName = TEXT("BossStatusViewModel");
	const FName MainHealthProgressBarName = TEXT("ProgressBar");
	const FName DelayedHealthProgressBarName = TEXT("ProgressBar_Delayed");

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

	void ApplyProgressBarLayerColors(UProgressBar& ProgressBar, const FLinearColor& CurrentColor, const FLinearColor& BackgroundColor)
	{
		ProgressBar.SetFillColorAndOpacity(CurrentColor);
		FProgressBarStyle LayerStyle = ProgressBar.GetWidgetStyle();
		LayerStyle.BackgroundImage.TintColor = FSlateColor(BackgroundColor);
		ProgressBar.SetWidgetStyle(LayerStyle);
	}

	float AdvanceLayerScaled(float Current, float Target, float DeltaTime, float InterpSpeed, float SnapThreshold)
	{
		// 회복이나 원본 교체로 목표가 올라갈 때에는 잔상을 남기지 않습니다
		if (Target >= Current)
		{
			return Target;
		}

		// 지수 보간은 목표에 정확히 닿지 않으므로 남은 간격이 눈에 띄지 않는 구간에서 끊어냅니다
		if (Current - Target <= SnapThreshold)
		{
			return Target;
		}

		return FMath::Max(FMath::FInterpTo(Current, Target, DeltaTime, InterpSpeed), Target);
	}

	FRSBossHealthLayerDisplay CalculateLayerDisplay(float LayerScaled, int32 LayerCount)
	{
		FRSBossHealthLayerDisplay LayerDisplay;
		if (LayerScaled <= 0.0f)
		{
			return LayerDisplay;
		}

		const int32 SafeLayerCount = FMath::Max(LayerCount, 1);
		LayerDisplay.RemainingLayerCount = FMath::Clamp(FMath::CeilToInt(LayerScaled), 1, SafeLayerCount);
		LayerDisplay.LayerPercent = FMath::Clamp(LayerScaled - (LayerDisplay.RemainingLayerCount - 1), 0.0f, 1.0f);
		return LayerDisplay;
	}

	float CalculateDelayedLayerPercent(float DelayedLayerScaled, int32 RemainingLayerCount)
	{
		// 체력이 0이면 남은 레이어가 0이므로 마지막 레이어를 기준으로 남은 잔상을 마저 비웁니다
		const int32 ActiveLayerIndex = FMath::Max(RemainingLayerCount, 1) - 1;
		return FMath::Clamp(DelayedLayerScaled - ActiveLayerIndex, 0.0f, 1.0f);
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

	const TScriptInterface<INotifyFieldValueChanged> ViewModelInterface = View->GetViewModel(RSBossHealthBar::BossStatusViewModelName);
	URSBossStatusViewModel* BossStatusViewModel = Cast<URSBossStatusViewModel>(ViewModelInterface.GetObject());
	if (!BossStatusViewModel)
	{
		return;
	}

	BoundBossStatusViewModel = BossStatusViewModel;
	BossStatusViewModel->OnHealthBarShakeRequested.AddUniqueDynamic(this, &ThisClass::RequestHealthBarShake);
	BossStatusViewModel->OnHealthBarShakeResetRequested.AddUniqueDynamic(this, &ThisClass::ResetHealthBarShake);
	HealthNormalizedChangedHandle = BossStatusViewModel->AddFieldValueChangedDelegate(
		URSBossStatusViewModel::FFieldNotificationClassDescriptor::HealthNormalized,
		INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateUObject(this, &ThisClass::HandleHealthLayerScaledChanged));

	// 처음 표시할 때 바가 흘러내리지 않도록 두 값 모두 현재 체력에서 시작합니다
	DisplayLayerScaled = BossStatusViewModel->GetHealthLayerScaled();
	DelayedLayerScaled = DisplayLayerScaled;
	DrainHoldRemaining = 0.0f;
	AppliedRemainingLayerCount = INDEX_NONE;
	ApplyLayerColors(RSBossHealthBar::CalculateLayerDisplay(DisplayLayerScaled, BossStatusViewModel->GetHealthLayerCount()).RemainingLayerCount);
}

void URSBossHealthBarWidget::UnbindFromBossStatusViewModel()
{
	if (URSBossStatusViewModel* BossStatusViewModel = BoundBossStatusViewModel.Get())
	{
		BossStatusViewModel->OnHealthBarShakeRequested.RemoveDynamic(this, &ThisClass::RequestHealthBarShake);
		BossStatusViewModel->OnHealthBarShakeResetRequested.RemoveDynamic(this, &ThisClass::ResetHealthBarShake);
		if (HealthNormalizedChangedHandle.IsValid())
		{
			BossStatusViewModel->RemoveFieldValueChangedDelegate(URSBossStatusViewModel::FFieldNotificationClassDescriptor::HealthNormalized, HealthNormalizedChangedHandle);
		}
	}

	HealthNormalizedChangedHandle.Reset();
	BoundBossStatusViewModel.Reset();
}

void URSBossHealthBarWidget::ApplyLayerColors(int32 DisplayRemainingLayerCount)
{
	const URSBossStatusViewModel* BossStatusViewModel = BoundBossStatusViewModel.Get();
	UProgressBar* MainHealthProgressBar = FindMainHealthProgressBar();
	if (!BossStatusViewModel || !MainHealthProgressBar || AppliedRemainingLayerCount == DisplayRemainingLayerCount)
	{
		return;
	}

	AppliedRemainingLayerCount = DisplayRemainingLayerCount;

	// 색 순번은 ViewModel의 실제 체력이 아니라 애니메이션이 도달한 레이어를 따라야 옛 레이어가 비는 동안 색이 유지됩니다
	const int32 CurrentColorIndex = DisplayRemainingLayerCount > 0 ? (BossStatusViewModel->GetHealthLayerCount() - DisplayRemainingLayerCount) % 4 : 0;
	const FLinearColor BackgroundColor = DisplayRemainingLayerCount >= 2 ? GetLayerColor(CurrentColorIndex + 1) : EmptyLayerColor;
	UProgressBar* DelayedHealthProgressBar = FindDelayedHealthProgressBar();
	if (!DelayedHealthProgressBar)
	{
		RSBossHealthBar::ApplyProgressBarLayerColors(*MainHealthProgressBar, GetLayerColor(CurrentColorIndex), BackgroundColor);
		return;
	}

	// 뒤에 있는 잔상이 비쳐 보이도록 남은 영역의 색은 잔상 바가 맡고 앞의 실제 바는 배경을 비웁니다
	RSBossHealthBar::ApplyProgressBarLayerColors(*DelayedHealthProgressBar, ChipColor, BackgroundColor);
	RSBossHealthBar::ApplyProgressBarLayerColors(*MainHealthProgressBar, GetLayerColor(CurrentColorIndex), FLinearColor::Transparent);
}

UProgressBar* URSBossHealthBarWidget::FindMainHealthProgressBar() const
{
	return WidgetTree ? Cast<UProgressBar>(WidgetTree->FindWidget(RSBossHealthBar::MainHealthProgressBarName)) : nullptr;
}

UProgressBar* URSBossHealthBarWidget::FindDelayedHealthProgressBar() const
{
	return WidgetTree ? Cast<UProgressBar>(WidgetTree->FindWidget(RSBossHealthBar::DelayedHealthProgressBarName)) : nullptr;
}

void URSBossHealthBarWidget::HandleHealthLayerScaledChanged(UObject*, UE::FieldNotification::FFieldId)
{
	const URSBossStatusViewModel* BossStatusViewModel = BoundBossStatusViewModel.Get();
	if (!BossStatusViewModel)
	{
		return;
	}

	if (BossStatusViewModel->GetHealthLayerScaled() < DelayedLayerScaled)
	{
		DrainHoldRemaining = DrainHoldDuration;
	}
}

void URSBossHealthBarWidget::UpdateHealthBarAnimation(float InDeltaTime)
{
	const URSBossStatusViewModel* BossStatusViewModel = BoundBossStatusViewModel.Get();
	UProgressBar* MainHealthProgressBar = FindMainHealthProgressBar();
	if (!BossStatusViewModel || !MainHealthProgressBar)
	{
		return;
	}

	const float TargetLayerScaled = BossStatusViewModel->GetHealthLayerScaled();
	DisplayLayerScaled = RSBossHealthBar::AdvanceLayerScaled(DisplayLayerScaled, TargetLayerScaled, InDeltaTime, DisplayInterpSpeed, DrainSnapThreshold);

	if (DrainHoldRemaining > 0.0f && DelayedLayerScaled > TargetLayerScaled)
	{
		DrainHoldRemaining -= InDeltaTime;
	}
	else
	{
		DrainHoldRemaining = 0.0f;
		DelayedLayerScaled = RSBossHealthBar::AdvanceLayerScaled(DelayedLayerScaled, TargetLayerScaled, InDeltaTime, DrainInterpSpeed, DrainSnapThreshold);
	}

	// 잔상이 실제 체력 바를 앞지르면 깎인 구간이 사라지므로 항상 뒤에 머물게 합니다
	DelayedLayerScaled = FMath::Max(DelayedLayerScaled, DisplayLayerScaled);

	const FRSBossHealthLayerDisplay LayerDisplay = RSBossHealthBar::CalculateLayerDisplay(DisplayLayerScaled, BossStatusViewModel->GetHealthLayerCount());
	ApplyLayerColors(LayerDisplay.RemainingLayerCount);
	MainHealthProgressBar->SetPercent(LayerDisplay.LayerPercent);
	if (UProgressBar* DelayedHealthProgressBar = FindDelayedHealthProgressBar())
	{
		DelayedHealthProgressBar->SetPercent(RSBossHealthBar::CalculateDelayedLayerPercent(DelayedLayerScaled, LayerDisplay.RemainingLayerCount));
	}
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

	UpdateHealthBarAnimation(InDeltaTime);

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
	ApplyShakeTranslation(RSBossHealthBar::CalculateTranslation(CurrentShakeStrength, NormalizedTime, MaxShakeX, MaxShakeY));
}

void URSBossHealthBarWidget::ApplyShakeTranslation(const FVector2D& Translation)
{
	if (WidgetTree && WidgetTree->RootWidget)
	{
		WidgetTree->RootWidget->SetRenderTranslation(Translation);
	}
}
