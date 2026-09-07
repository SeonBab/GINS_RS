// Fill out your copyright notice in the Description page of Project Settings.


#include "RSDamageNumberLayerWidget.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "RSDamageNumberViewModel.h"
#include "RSDamageNumberWidget.h"
#include "RSLocalPlayerViewModelSubsystem.h"

void URSDamageNumberLayerWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 루트가 곧 배치 캔버스입니다. 이름 규칙을 요구하지 않으므로 에셋에서 이름을 맞출 필요가 없습니다
	RootCanvasPanel = Cast<UCanvasPanel>(GetRootWidget());

	// 위젯 컴포넌트와 디자이너 미리보기는 로컬 플레이어가 없는 월드에서도 위젯을 만듭니다
	const UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}

	BindToDamageNumberViewModel();

	// 배선이 빠지면 숫자가 아무것도 뜨지 않고 원인도 남지 않으므로 생성 시 한 번만 확인합니다
	if (!BoundDamageNumberViewModel.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s found no URSDamageNumberViewModel"), *GetNameSafe(this));
	}

	if (!DamageNumberWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s has no DamageNumberWidgetClass"), *GetNameSafe(this));
	}

	if (!RootCanvasPanel)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s needs a CanvasPanel as its root widget"), *GetNameSafe(this));
	}
}

void URSDamageNumberLayerWidget::NativeDestruct()
{
	UnbindFromDamageNumberViewModel();
	HandleDamageNumberResetRequested();

	RootCanvasPanel = nullptr;

	Super::NativeDestruct();
}

void URSDamageNumberLayerWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 고정된 값을 매 프레임 다시 계산하지 않도록 표시 중인 항목이 있을 때만 갱신합니다
	if (ActiveDamageNumbers.IsEmpty())
	{
		return;
	}

	for (int32 ActiveIndex = ActiveDamageNumbers.Num() - 1; ActiveIndex >= 0; --ActiveIndex)
	{
		FRSActiveDamageNumber& ActiveDamageNumber = ActiveDamageNumbers[ActiveIndex];
		ActiveDamageNumber.ElapsedSeconds += InDeltaTime;

		if (!ActiveDamageNumber.DamageNumberWidget || ActiveDamageNumber.ElapsedSeconds >= LifetimeSeconds)
		{
			ReleaseActiveDamageNumber(ActiveIndex);

			continue;
		}

		UpdateActiveDamageNumber(ActiveDamageNumber);
	}
}

void URSDamageNumberLayerWidget::BindToDamageNumberViewModel()
{
	UnbindFromDamageNumberViewModel();

	// MVVM Source 주입은 사용하지 않습니다
	// 이 위젯은 FieldNotify를 바인딩하지 않고 요청 델리게이트만 구독하므로 컴파일된 MVVM 뷰 클래스가 생성되지 않고,
	// 그러면 Source를 선언해도 런타임에 전달되지 않습니다. URSOverheadHealthBarWidget과 같은 경로로 직접 얻습니다
	const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	if (!LocalPlayer)
	{
		return;
	}

	URSLocalPlayerViewModelSubsystem* ViewModelSubsystem = LocalPlayer->GetSubsystem<URSLocalPlayerViewModelSubsystem>();
	URSDamageNumberViewModel* DamageNumberViewModel = ViewModelSubsystem ? ViewModelSubsystem->GetOrCreateViewModel<URSDamageNumberViewModel>() : nullptr;
	if (!DamageNumberViewModel)
	{
		return;
	}

	BoundDamageNumberViewModel = DamageNumberViewModel;
	DamageNumberViewModel->OnDamageNumberRequested.AddUniqueDynamic(this, &ThisClass::HandleDamageNumberRequested);
	DamageNumberViewModel->OnDamageNumberResetRequested.AddUniqueDynamic(this, &ThisClass::HandleDamageNumberResetRequested);
}

void URSDamageNumberLayerWidget::UnbindFromDamageNumberViewModel()
{
	if (URSDamageNumberViewModel* DamageNumberViewModel = BoundDamageNumberViewModel.Get())
	{
		DamageNumberViewModel->OnDamageNumberRequested.RemoveDynamic(this, &ThisClass::HandleDamageNumberRequested);
		DamageNumberViewModel->OnDamageNumberResetRequested.RemoveDynamic(this, &ThisClass::HandleDamageNumberResetRequested);
	}

	BoundDamageNumberViewModel.Reset();
}

void URSDamageNumberLayerWidget::HandleDamageNumberRequested(FText DisplayDamageText, FVector WorldAnchor)
{
	URSDamageNumberWidget* DamageNumberWidget = AcquireDamageNumberWidget();
	if (!DamageNumberWidget)
	{
		return;
	}

	DamageNumberWidget->SetDamageText(DisplayDamageText);

	// 이전 사용의 시각 상태가 남지 않도록 활성화 시점에 완전히 초기화합니다
	DamageNumberWidget->SetRenderOpacity(1.0f);

	// 위치는 첫 투영에서 결정되므로 그때까지 숨겨 이전 사용 위치에서 한 프레임 보이는 것을 막습니다
	DamageNumberWidget->SetVisibility(ESlateVisibility::Hidden);

	FRSActiveDamageNumber ActiveDamageNumber;
	ActiveDamageNumber.WorldAnchor = WorldAnchor;
	ActiveDamageNumber.ElapsedSeconds = 0.0f;
	ActiveDamageNumber.HorizontalScatter = FMath::FRandRange(-HorizontalScatterRange, HorizontalScatterRange);
	ActiveDamageNumber.DamageNumberWidget = DamageNumberWidget;

	ActiveDamageNumbers.Add(ActiveDamageNumber);
}

void URSDamageNumberLayerWidget::HandleDamageNumberResetRequested()
{
	for (int32 ActiveIndex = ActiveDamageNumbers.Num() - 1; ActiveIndex >= 0; --ActiveIndex)
	{
		ReleaseActiveDamageNumber(ActiveIndex);
	}
}

URSDamageNumberWidget* URSDamageNumberLayerWidget::AcquireDamageNumberWidget()
{
	if (!DamageNumberWidgetClass || !RootCanvasPanel)
	{
		return nullptr;
	}

	// 상한에 닿으면 새로 만들지 않고 가장 오래된 항목을 회수해 그 위젯을 다시 씁니다
	if (DamageNumberWidgetPool.IsEmpty() && CreatedDamageNumberWidgetCount >= MaxConcurrentNumbers && !ActiveDamageNumbers.IsEmpty())
	{
		ReleaseActiveDamageNumber(0);
	}

	if (!DamageNumberWidgetPool.IsEmpty())
	{
		return DamageNumberWidgetPool.Pop();
	}

	URSDamageNumberWidget* DamageNumberWidget = CreateWidget<URSDamageNumberWidget>(this, DamageNumberWidgetClass);
	if (!DamageNumberWidget)
	{
		return nullptr;
	}

	++CreatedDamageNumberWidgetCount;

	// 슬롯 정렬은 전달한 위치가 숫자의 시각적 중심이 되도록 고정합니다
	// AutoSize가 없으면 중심이 텍스트가 아니라 슬롯 크기 기준으로 잡힙니다
	if (UCanvasPanelSlot* CanvasSlot = RootCanvasPanel->AddChildToCanvas(DamageNumberWidget))
	{
		CanvasSlot->SetAutoSize(true);
		CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	}

	return DamageNumberWidget;
}

void URSDamageNumberLayerWidget::ReleaseActiveDamageNumber(int32 ActiveIndex)
{
	if (!ActiveDamageNumbers.IsValidIndex(ActiveIndex))
	{
		return;
	}

	if (URSDamageNumberWidget* DamageNumberWidget = ActiveDamageNumbers[ActiveIndex].DamageNumberWidget)
	{
		DamageNumberWidget->SetVisibility(ESlateVisibility::Collapsed);
		DamageNumberWidgetPool.Add(DamageNumberWidget);
	}

	// 오래된 순서를 유지해야 상한에 닿았을 때 회수 대상이 정해집니다
	ActiveDamageNumbers.RemoveAt(ActiveIndex);
}

void URSDamageNumberLayerWidget::UpdateActiveDamageNumber(FRSActiveDamageNumber& ActiveDamageNumber)
{
	URSDamageNumberWidget* DamageNumberWidget = ActiveDamageNumber.DamageNumberWidget;

	FVector2D ProjectedPosition = FVector2D::ZeroVector;

	// 캔버스가 전체 Game Viewport 좌표계를 사용하므로 플레이어 뷰포트 부분 영역 기준이 필요하지 않습니다
	const bool bIsProjected = UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(GetOwningPlayer(), ActiveDamageNumber.WorldAnchor, ProjectedPosition, false);
	if (!bIsProjected)
	{
		// 카메라 뒤로 넘어간 것은 일시적인 숨김이며 항목의 종료가 아니므로 수명은 그대로 진행합니다
		DamageNumberWidget->SetVisibility(ESlateVisibility::Hidden);

		return;
	}

	UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(DamageNumberWidget->Slot);
	if (!CanvasSlot)
	{
		return;
	}

	const float NormalizedTime = FMath::Clamp(ActiveDamageNumber.ElapsedSeconds / LifetimeSeconds, 0.0f, 1.0f);

	// 앵커는 월드에 고정하고 상승과 흩기는 투영 이후의 Widget 좌표에서만 적용합니다
	// Widget 좌표의 Y는 아래로 증가하므로 상승은 음수 Y입니다
	const FVector2D PresentationOffset(ActiveDamageNumber.HorizontalScatter, -RiseDistance * NormalizedTime);

	CanvasSlot->SetPosition(ProjectedPosition + PresentationOffset);
	DamageNumberWidget->SetRenderOpacity(1.0f - NormalizedTime);
	DamageNumberWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
}
