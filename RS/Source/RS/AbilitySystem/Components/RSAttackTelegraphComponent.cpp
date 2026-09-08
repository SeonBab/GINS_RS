// Fill out your copyright notice in the Description page of Project Settings.

#include "RSAttackTelegraphComponent.h"

#include "Components/DecalComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

URSAttackTelegraphComponent::URSAttackTelegraphComponent()
{
	// 표시가 없는 동안 Tick 비용을 내지 않도록 활성 슬롯이 생길 때만 켭니다
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void URSAttackTelegraphComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	for (FRSTelegraphSlot& Slot : Slots)
	{
		if (Slot.bIsActive)
		{
			UpdateSlot(Slot, DeltaTime);
		}
	}

	RefreshTickEnabled();
}

void URSAttackTelegraphComponent::OnUnregister()
{
	HideAllShapes();

	Super::OnUnregister();
}

void URSAttackTelegraphComponent::ShowShape(const FRSCombatShape& Shape, const FTransform& ShapeTransform, const FRSTelegraphPresentation& Presentation)
{
	// 잘못된 Cone을 Clamp하거나 다른 형상처럼 표시하면 실제 판정과 예고가 달라지므로 표시하지 않습니다
	if (Shape.Type == ERSCombatShapeType::Cone && !Shape.IsDataValid())
	{
		return;
	}

	// 머티리얼이 준비되기 전까지 개발 중에도 표시를 확인할 수 있도록 판정과 같은 형상 데이터로 대신 그립니다
	// 머티리얼을 지정하면 이 경로는 쓰이지 않습니다
	if (!DecalMaterial)
	{
		URSCombatFunctionLibrary::DrawDebugCombatShape(GetWorld(), Shape, ShapeTransform, FColor::Yellow, Presentation.HoldDuration);

		return;
	}

	FRSTelegraphSlot& Slot = AcquireSlot();
	Slot.Shape = Shape;
	Slot.Presentation = Presentation;
	Slot.ElapsedTime = 0.0f;
	Slot.bIsActive = true;

	SetUpSlotDecal(Slot, ShapeTransform);

	// 첫 프레임부터 올바른 값이 보이도록 Tick을 기다리지 않고 한 번 갱신합니다
	UpdateSlot(Slot, 0.0f);

	RefreshTickEnabled();
}

void URSAttackTelegraphComponent::HideAllShapes()
{
	for (FRSTelegraphSlot& Slot : Slots)
	{
		ReleaseSlot(Slot);
	}

	RefreshTickEnabled();
}

FRSTelegraphSlot& URSAttackTelegraphComponent::AcquireSlot()
{
	for (FRSTelegraphSlot& Slot : Slots)
	{
		if (!Slot.bIsActive)
		{
			return Slot;
		}
	}

	return Slots.AddDefaulted_GetRef();
}

void URSAttackTelegraphComponent::SetUpSlotDecal(FRSTelegraphSlot& Slot, const FTransform& ShapeTransform)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	if (!Slot.Decal)
	{
		Slot.Decal = NewObject<UDecalComponent>(Owner);
		Slot.Decal->SetupAttachment(Owner->GetRootComponent());
		Slot.Decal->RegisterComponent();

		// 예고한 자리와 판정 자리가 갈라지지 않도록 소유자를 따라가지 않고 월드에 고정합니다
		Slot.Decal->SetUsingAbsoluteLocation(true);
		Slot.Decal->SetUsingAbsoluteRotation(true);
	}

	if (!Slot.MaterialInstance)
	{
		Slot.MaterialInstance = UMaterialInstanceDynamic::Create(DecalMaterial, this);
		Slot.Decal->SetDecalMaterial(Slot.MaterialInstance);
	}

	// DecalSize가 곧 판정 범위이므로 형상에서 파생시켜 표시와 판정이 1:1로 대응하게 합니다
	// X는 투영 깊이, Y와 Z는 투영 사각형의 반크기입니다
	// Box 형상은 아직 예고를 쓰는 소비자가 없어 가장 긴 축으로 정사각형을 만드는 임시 값입니다
	float ShapeHalfSize = Slot.Shape.BoxExtent.GetMax();
	if (Slot.Shape.Type == ERSCombatShapeType::Sphere)
	{
		ShapeHalfSize = Slot.Shape.Radius;
	}
	else if (Slot.Shape.Type == ERSCombatShapeType::Cone)
	{
		ShapeHalfSize = Slot.Shape.Range;
	}
	Slot.Decal->DecalSize = FVector(ProjectionDepth, ShapeHalfSize, ShapeHalfSize);

	// DecalSize에는 Setter가 없어 직접 대입하므로, 슬롯을 재사용할 때 이전 크기가 남지 않도록 렌더 상태를 무효화합니다
	Slot.Decal->MarkRenderStateDirty();

	// 직접 만든 DecalComponent는 회전을 처리하지 않으므로 바닥을 향하도록 직접 눕힙니다
	const FRotator GroundProjectionRotation(-90.0f, ShapeTransform.GetRotation().Rotator().Yaw, 0.0f);
	Slot.Decal->SetWorldLocationAndRotation(ShapeTransform.GetLocation(), GroundProjectionRotation);

	const bool bUseConeMask = Slot.Shape.Type == ERSCombatShapeType::Cone;
	const float InnerRatio = Slot.Shape.Type == ERSCombatShapeType::Sphere && Slot.Shape.Radius > 0.0f
		? Slot.Shape.InnerRadius / Slot.Shape.Radius
		: 0.0f;
	const float ConeHalfAngleCos = bUseConeMask
		? FMath::Cos(FMath::DegreesToRadians(Slot.Shape.Angle * 0.5f))
		: 1.0f;

	// 슬롯의 MID는 다른 Shape가 재사용할 수 있으므로 Shape 관련 값을 항상 완전한 상태로 다시 씁니다
	Slot.MaterialInstance->SetScalarParameterValue(UseConeMaskParameterName, bUseConeMask ? 1.0f : 0.0f);
	Slot.MaterialInstance->SetScalarParameterValue(InnerRatioParameterName, InnerRatio);
	Slot.MaterialInstance->SetScalarParameterValue(ConeHalfAngleCosParameterName, ConeHalfAngleCos);

	Slot.Decal->SetVisibility(true);
}

void URSAttackTelegraphComponent::UpdateSlot(FRSTelegraphSlot& Slot, float DeltaTime)
{
	Slot.ElapsedTime += DeltaTime;

	const FRSTelegraphPresentation& Presentation = Slot.Presentation;

	// Fade 없이 유지 시간이 끝나는 즉시 슬롯을 되돌립니다
	if (Slot.ElapsedTime >= Presentation.HoldDuration)
	{
		ReleaseSlot(Slot);

		return;
	}

	const float Fill = Presentation.FillDuration > 0.0f ? FMath::Min(Slot.ElapsedTime / Presentation.FillDuration, 1.0f) : 1.0f;

	if (Slot.MaterialInstance)
	{
		Slot.MaterialInstance->SetScalarParameterValue(AlphaParameterName, FMath::Clamp(Presentation.Opacity, 0.0f, 1.0f));
		Slot.MaterialInstance->SetScalarParameterValue(FillParameterName, Fill);
	}
}

void URSAttackTelegraphComponent::ReleaseSlot(FRSTelegraphSlot& Slot)
{
	Slot.bIsActive = false;
	Slot.ElapsedTime = 0.0f;

	// 데칼과 Dynamic Material Instance는 다음 표시에서 재사용하므로 숨기기만 합니다
	if (Slot.Decal)
	{
		Slot.Decal->SetVisibility(false);
	}
}

void URSAttackTelegraphComponent::RefreshTickEnabled()
{
	bool bHasActiveSlot = false;
	for (const FRSTelegraphSlot& Slot : Slots)
	{
		if (Slot.bIsActive)
		{
			bHasActiveSlot = true;

			break;
		}
	}

	SetComponentTickEnabled(bHasActiveSlot);
}
