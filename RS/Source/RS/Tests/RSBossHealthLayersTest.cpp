#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "RSBossStatusViewModel.h"
#include "RSHealthComponent.h"
#include "RSAbilitySystemComponent.h"
#include "RSHealthSet.h"
#include <cmath>
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSBossHealthLayersTest, "RS.UI.BossHealthLayers", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSBossHealthLayersTest::RunTest(const FString& Parameters)
{
	URSBossStatusViewModel* ViewModel = NewObject<URSBossStatusViewModel>();
	ViewModel->HealthLayerCount = 5;
	const float HealthValues[] = { 5000.0f, 4500.0f, 4000.0f, 3250.0f, 3000.0f, 2000.0f, 1000.0f, 500.0f, 0.0f };
	const int32 RemainingCounts[] = { 5, 5, 4, 4, 3, 2, 1, 1, 0 };
	const float LayerPercents[] = { 1.0f, 0.5f, 1.0f, 0.25f, 1.0f, 1.0f, 1.0f, 0.5f, 0.0f };
	const int32 LayerColorIndices[] = { 0, 0, 1, 1, 2, 3, 0, 0, 0 };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(HealthValues); ++Index)
	{
		ViewModel->ApplyHealthValues(HealthValues[Index], 5000.0f);
		const FString Context = FString::Printf(TEXT("Health %.0f"), HealthValues[Index]);
		TestEqual(Context + TEXT(" remaining"), ViewModel->RemainingLayerCount, RemainingCounts[Index]);
		TestEqual(Context + TEXT(" layer percent"), ViewModel->CurrentLayerPercent, LayerPercents[Index]);
		TestEqual(Context + TEXT(" layer color"), ViewModel->CurrentLayerColorIndex, LayerColorIndices[Index]);
		TestEqual(Context + TEXT(" total percent"), ViewModel->HealthNormalized, HealthValues[Index] / 5000.0f);
		TestEqual(Context + TEXT(" count visibility"), ViewModel->LayerCountVisibility, RemainingCounts[Index] >= 2 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		TestEqual(Context + TEXT(" count text"), ViewModel->LayerCountText.ToString(), RemainingCounts[Index] >= 2 ? FString::Printf(TEXT("x%d"), RemainingCounts[Index]) : FString());
	}

	// 한 번에 여러 레이어를 관통해도 직전 표시 상태가 계산에 관여하지 않아야 합니다
	ViewModel->ApplyHealthValues(5000.0f, 5000.0f);
	ViewModel->ApplyHealthValues(500.0f, 5000.0f);
	TestEqual(TEXT("Multi-layer damage"), ViewModel->RemainingLayerCount, 1);
	TestEqual(TEXT("Multi-layer damage percent"), ViewModel->CurrentLayerPercent, 0.5f);

	for (const int32 Count : { 1, 0, -3 })
	{
		ViewModel->HealthLayerCount = Count;
		ViewModel->ApplyHealthValues(25.0f, 100.0f);
		TestEqual(TEXT("Single or invalid layer count"), ViewModel->CurrentLayerPercent, 0.25f);
		TestTrue(TEXT("Single layer text hidden"), ViewModel->LayerCountText.IsEmpty());
	}

	// 반올림되는 비정수 경계와 그 양옆의 인접 float 값을 구분합니다
	ViewModel->HealthLayerCount = 6;
	const float Boundary = 100.0f / 6.0f;
	ViewModel->ApplyHealthValues(Boundary, 100.0f);
	TestEqual(TEXT("Fractional boundary count"), ViewModel->RemainingLayerCount, 1);
	TestEqual(TEXT("Fractional boundary full"), ViewModel->CurrentLayerPercent, 1.0f);
	ViewModel->ApplyHealthValues(std::nextafter(Boundary, 100.0f), 100.0f);
	TestEqual(TEXT("Above boundary remains second layer"), ViewModel->RemainingLayerCount, 2);
	TestTrue(TEXT("Above boundary retains positive health"), ViewModel->CurrentLayerPercent > 0.0f);
	ViewModel->ApplyHealthValues(std::nextafter(Boundary, 0.0f), 100.0f);
	TestEqual(TEXT("Below boundary stays first layer"), ViewModel->RemainingLayerCount, 1);
	TestTrue(TEXT("Below boundary is not full"), ViewModel->CurrentLayerPercent < 1.0f);

	ViewModel->ApplyHealthValues(50.0f, 0.0f);
	TestEqual(TEXT("Invalid max clears layers"), ViewModel->RemainingLayerCount, 0);
	ViewModel->ApplyHealthValues(std::numeric_limits<float>::quiet_NaN(), 100.0f);
	TestEqual(TEXT("Nonfinite health clears layers"), ViewModel->RemainingLayerCount, 0);
	ViewModel->HealthLayerCount = MAX_int32;
	ViewModel->ApplyHealthValues(100.0f, 100.0f);
	TestEqual(TEXT("Maximum integer count does not overflow"), ViewModel->RemainingLayerCount, MAX_int32);

	// 원본 재등록과 교체에서 구독을 중복하거나 이전 카운트를 남기지 않아야 합니다
	URSHealthComponent* FirstSource = NewObject<URSHealthComponent>();
	URSHealthComponent* SecondSource = NewObject<URSHealthComponent>();
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
	AActor* FirstOwner = TestWorld->SpawnActor<AActor>();
	AActor* SecondOwner = TestWorld->SpawnActor<AActor>();
	URSAbilitySystemComponent* FirstAbilitySystem = NewObject<URSAbilitySystemComponent>(FirstOwner);
	URSAbilitySystemComponent* SecondAbilitySystem = NewObject<URSAbilitySystemComponent>(SecondOwner);
	FirstAbilitySystem->RegisterComponent();
	SecondAbilitySystem->RegisterComponent();
	FirstAbilitySystem->InitAbilityActorInfo(FirstOwner, FirstOwner);
	SecondAbilitySystem->InitAbilityActorInfo(SecondOwner, SecondOwner);
	FirstAbilitySystem->AddAttributeSetSubobject(NewObject<URSHealthSet>(FirstAbilitySystem));
	SecondAbilitySystem->AddAttributeSetSubobject(NewObject<URSHealthSet>(SecondAbilitySystem));
	FirstSource->InitializeWithAbilitySystem(FirstAbilitySystem);
	SecondSource->InitializeWithAbilitySystem(SecondAbilitySystem);
	ViewModel->InitializeViewModel(FirstSource, FText::FromString(TEXT("First")), 5);
	TestEqual(TEXT("Initial source synchronizes full health"), ViewModel->RemainingLayerCount, 5);
	FirstAbilitySystem->SetNumericAttributeBase(URSHealthSet::GetHealthAttribute(), 65.0f);
	TestEqual(TEXT("ASC event updates remaining layers"), ViewModel->RemainingLayerCount, 4);
	TestEqual(TEXT("ASC event updates layer percent"), ViewModel->CurrentLayerPercent, 0.25f);
	ViewModel->InitializeViewModel(FirstSource, FText::FromString(TEXT("First")), 2);
	TestEqual(TEXT("Reregister updates layer setting"), ViewModel->HealthLayerCount, 2);
	TestEqual(TEXT("Reregister recomputes remaining count"), ViewModel->RemainingLayerCount, 2);
	ViewModel->InitializeViewModel(SecondSource, FText::FromString(TEXT("Second")), 3);
	SecondAbilitySystem->SetNumericAttributeBase(URSHealthSet::GetHealthAttribute(), 75.0f);
	FirstAbilitySystem->SetNumericAttributeBase(URSHealthSet::GetHealthAttribute(), 10.0f);
	TestEqual(TEXT("Old source no longer drives values"), ViewModel->Health, 75.0f);
	ViewModel->UninitializeViewModel();
	TestFalse(TEXT("Uninitialize hides boss"), ViewModel->bIsVisible);
	TestEqual(TEXT("Uninitialize clears layer percent"), ViewModel->CurrentLayerPercent, 0.0f);
	TestEqual(TEXT("Uninitialize clears count"), ViewModel->RemainingLayerCount, 0);
	TestTrue(TEXT("Uninitialize clears text"), ViewModel->LayerCountText.IsEmpty());
	TestEqual(TEXT("Uninitialize hides count"), ViewModel->LayerCountVisibility, ESlateVisibility::Collapsed);
	SecondAbilitySystem->SetNumericAttributeBase(URSHealthSet::GetHealthAttribute(), 25.0f);
	TestEqual(TEXT("Disconnected source cannot restore health"), ViewModel->Health, 0.0f);
	ViewModel->InitializeViewModel(FirstSource, FText::FromString(TEXT("First")), 5);
	TestEqual(TEXT("Reentry reads current source health"), ViewModel->Health, 10.0f);
	TestEqual(TEXT("Reentry reads current source layers"), ViewModel->RemainingLayerCount, 1);
	ViewModel->UninitializeViewModel();
	FirstSource->UninitializeFromAbilitySystem();
	SecondSource->UninitializeFromAbilitySystem();
	TestWorld->DestroyWorld(false);
	return true;
}

#endif
