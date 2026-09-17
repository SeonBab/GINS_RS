#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "RSInteractionSelection.h"
#include "RSInteractionSelectionTestTypes.h"

namespace
{
	/** 선정 규칙 테스트용 임시 월드를 만듭니다 */
	UWorld* CreateInteractionSelectionTestWorld()
	{
		const FName WorldName = MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("RSInteractionSelectionTestWorld"), EUniqueObjectNameOptions::GloballyUnique);
		FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
		UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false, WorldName, GetTransientPackage());
		TestWorld->AddToRoot();
		WorldContext.SetCurrentWorld(TestWorld);
		TestWorld->InitializeActorsForPlay(FURL());

		return TestWorld;
	}

	/** 테스트 월드가 남긴 WorldContext와 Root 참조를 함께 정리합니다 */
	void DestroyInteractionSelectionTestWorld(UWorld* TestWorld)
	{
		if (!TestWorld)
		{
			return;
		}

		TestWorld->RemoveFromRoot();
		GEngine->DestroyWorldContext(TestWorld);
		TestWorld->DestroyWorld(false);
	}

	/** 지정한 위치에 상호작용 대상을 배치합니다 */
	ARSInteractionSelectionTestTarget* SpawnTarget(UWorld& TestWorld, const FVector& Location)
	{
		return TestWorld.SpawnActor<ARSInteractionSelectionTestTarget>(ARSInteractionSelectionTestTarget::StaticClass(), FTransform(Location));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSInteractionNearestSelectionTest, "RS.Interaction.Selection.Nearest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSInteractionNearestSelectionTest::RunTest(const FString& Parameters)
{
	UWorld* TestWorld = CreateInteractionSelectionTestWorld();
	TestNotNull(TEXT("Interaction selection test world"), TestWorld);
	if (!TestWorld)
	{
		return false;
	}

	const FVector ScanOrigin = FVector::ZeroVector;
	AActor* Scanner = TestWorld->SpawnActor<AActor>();

	TestNull(TEXT("No candidates yield no focus"), RSInteraction::FindNearestInteractable({}, ScanOrigin, Scanner));

	ARSInteractionSelectionTestTarget* NearTarget = SpawnTarget(*TestWorld, FVector(200.0f, 0.0f, 0.0f));
	ARSInteractionSelectionTestTarget* FarTarget = SpawnTarget(*TestWorld, FVector(600.0f, 0.0f, 0.0f));
	ARSInteractionSelectionTestNonTarget* NonTarget = TestWorld->SpawnActor<ARSInteractionSelectionTestNonTarget>(ARSInteractionSelectionTestNonTarget::StaticClass(), FTransform(FVector(50.0f, 0.0f, 0.0f)));

	TestNotNull(TEXT("Near target"), NearTarget);
	TestNotNull(TEXT("Far target"), FarTarget);
	TestNotNull(TEXT("Non target"), NonTarget);
	if (!NearTarget || !FarTarget || !NonTarget)
	{
		DestroyInteractionSelectionTestWorld(TestWorld);

		return false;
	}

	TArray<UPrimitiveComponent*> Candidates;
	Candidates.Add(FarTarget->GetShape());
	Candidates.Add(NearTarget->GetShape());

	TestEqual(TEXT("The nearest interactable is selected regardless of candidate order"), RSInteraction::FindNearestInteractable(Candidates, ScanOrigin, Scanner), Cast<AActor>(NearTarget));

	// 인터페이스를 구현하지 않은 액터는 더 가까워도 선택되지 않아야 합니다
	Candidates.Add(NonTarget->GetShape());
	TestEqual(TEXT("An actor without the interface is never selected"), RSInteraction::FindNearestInteractable(Candidates, ScanOrigin, Scanner), Cast<AActor>(NearTarget));

	// 형상 표면 기준이므로 중심이 더 먼 큰 대상이 바로 앞에 있으면 그쪽이 선택되어야 합니다
	FarTarget->GetShape()->SetSphereRadius(550.0f);
	TestEqual(TEXT("A large shape close to the origin wins over a small distant one"), RSInteraction::FindNearestInteractable(Candidates, ScanOrigin, Scanner), Cast<AActor>(FarTarget));

	FarTarget->GetShape()->SetSphereRadius(50.0f);

	// 같은 액터의 콜리전이 여러 개 걸려도 결과는 액터 하나입니다
	Candidates.Add(NearTarget->GetShape());
	TestEqual(TEXT("Duplicate components of one actor still resolve to that actor"), RSInteraction::FindNearestInteractable(Candidates, ScanOrigin, Scanner), Cast<AActor>(NearTarget));

	NearTarget->SetCanInteract(false);
	TestEqual(TEXT("A target refusing interaction falls back to the next candidate"), RSInteraction::FindNearestInteractable(Candidates, ScanOrigin, Scanner), Cast<AActor>(FarTarget));

	FarTarget->SetCanInteract(false);
	TestNull(TEXT("No focus remains when every candidate refuses"), RSInteraction::FindNearestInteractable(Candidates, ScanOrigin, Scanner));

	DestroyInteractionSelectionTestWorld(TestWorld);

	return true;
}

#endif
