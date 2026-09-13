#include "RSHitFlashComponent.h"

#include "Components/MeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "RSHealthComponent.h"

namespace RSHitFlash
{
	const FName AmountParameterName = TEXT("HitFlashAmount");

	float CalculateAmount(float NormalizedTime)
	{
		const float ClampedTime = FMath::Clamp(NormalizedTime, 0.0f, 1.0f);
		return FMath::Square(1.0f - ClampedTime);
	}
}

URSHitFlashComponent::URSHitFlashComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void URSHitFlashComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bIsHitFlashActive || !HitFlashMaterialInstance || HitFlashDuration <= 0.0f)
	{
		ResetHitFlash();
		return;
	}

	HitFlashElapsed += DeltaTime;
	const float NormalizedTime = FMath::Clamp(HitFlashElapsed / HitFlashDuration, 0.0f, 1.0f);
	if (NormalizedTime >= 1.0f)
	{
		ResetHitFlash();
		return;
	}

	HitFlashMaterialInstance->SetScalarParameterValue(RSHitFlash::AmountParameterName, RSHitFlash::CalculateAmount(NormalizedTime));
}

void URSHitFlashComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Uninitialize();

	Super::EndPlay(EndPlayReason);
}

void URSHitFlashComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	Uninitialize();

	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

void URSHitFlashComponent::Initialize(URSHealthComponent* InHealthComponent, UMeshComponent* InTargetMeshComponent)
{
	Uninitialize();

	ObservedHealthComponent = InHealthComponent;
	TargetMeshComponent = InTargetMeshComponent;
	if (ObservedHealthComponent)
	{
		ObservedHealthComponent->OnHealthChanged.AddUniqueDynamic(this, &ThisClass::HandleHealthChanged);
	}
}

void URSHitFlashComponent::Uninitialize()
{
	if (ObservedHealthComponent)
	{
		ObservedHealthComponent->OnHealthChanged.RemoveDynamic(this, &ThisClass::HandleHealthChanged);
	}

	ResetHitFlash();
	HitFlashMaterialInstance = nullptr;
	ObservedHealthComponent = nullptr;
	TargetMeshComponent = nullptr;
}

void URSHitFlashComponent::HandleHealthChanged(URSHealthComponent* HealthComponent, float OldValue, float NewValue)
{
	if (HealthComponent != ObservedHealthComponent || !FMath::IsFinite(OldValue) || !FMath::IsFinite(NewValue) || NewValue >= OldValue || !EnsureMaterialInstance())
	{
		return;
	}

	if (!bIsHitFlashActive)
	{
		PreviousOverlayMaterial = TargetMeshComponent->GetOverlayMaterial();
	}

	HitFlashElapsed = 0.0f;
	bIsHitFlashActive = true;
	HitFlashMaterialInstance->SetScalarParameterValue(RSHitFlash::AmountParameterName, 1.0f);
	TargetMeshComponent->SetOverlayMaterial(HitFlashMaterialInstance);
	SetComponentTickEnabled(true);
}

bool URSHitFlashComponent::EnsureMaterialInstance()
{
	if (HitFlashMaterialInstance)
	{
		return IsValid(TargetMeshComponent);
	}

	if (!HitFlashMaterial || !TargetMeshComponent)
	{
		return false;
	}

	HitFlashMaterialInstance = UMaterialInstanceDynamic::Create(HitFlashMaterial, this);
	return IsValid(HitFlashMaterialInstance);
}

void URSHitFlashComponent::ResetHitFlash()
{
	if (TargetMeshComponent && TargetMeshComponent->GetOverlayMaterial() == HitFlashMaterialInstance)
	{
		TargetMeshComponent->SetOverlayMaterial(PreviousOverlayMaterial);
	}

	if (HitFlashMaterialInstance)
	{
		HitFlashMaterialInstance->SetScalarParameterValue(RSHitFlash::AmountParameterName, 0.0f);
	}

	PreviousOverlayMaterial = nullptr;
	HitFlashElapsed = 0.0f;
	bIsHitFlashActive = false;
	SetComponentTickEnabled(false);
}

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSHitFlashAmountTest, "RS.Combat.HitFlash.Amount", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSHitFlashAmountTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Hit flash starts at full brightness"), RSHitFlash::CalculateAmount(0.0f), 1.0f);
	TestEqual(TEXT("Squared ease out reaches quarter brightness halfway"), RSHitFlash::CalculateAmount(0.5f), 0.25f);
	TestEqual(TEXT("Hit flash reaches zero at the end"), RSHitFlash::CalculateAmount(1.0f), 0.0f);
	TestEqual(TEXT("Negative time clamps to the start"), RSHitFlash::CalculateAmount(-1.0f), 1.0f);
	TestEqual(TEXT("Time beyond the duration clamps to the end"), RSHitFlash::CalculateAmount(2.0f), 0.0f);

	return true;
}

#endif
