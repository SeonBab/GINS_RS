#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "RSAbilitySystemComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSHitStopTest, "RS.Combat.HitStop", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSHitStopTest::RunTest(const FString& Parameters)
{
	const FName WorldName = MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("RSHitStopTestWorld"), EUniqueObjectNameOptions::GloballyUnique);
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false, WorldName, GetTransientPackage());
	TestWorld->AddToRoot();
	WorldContext.SetCurrentWorld(TestWorld);
	TestWorld->InitializeActorsForPlay(FURL());

	// 플레이어 ASC는 PlayerState가, Avatar는 Character가 맡으므로 소유자와 Avatar를 다른 액터로 둡니다
	// 그래야 Avatar만 먼저 사라지는 실제 상황을 재현할 수 있습니다
	AActor* OwnerActor = TestWorld->SpawnActor<AActor>();
	AActor* AvatarActor = TestWorld->SpawnActor<AActor>();
	TestNotNull(TEXT("Owner actor"), OwnerActor);
	TestNotNull(TEXT("Avatar actor"), AvatarActor);
	if (!OwnerActor || !AvatarActor)
	{
		TestWorld->RemoveFromRoot();
		GEngine->DestroyWorldContext(TestWorld);
		TestWorld->DestroyWorld(false);

		return false;
	}

	URSAbilitySystemComponent* AbilitySystemComp = NewObject<URSAbilitySystemComponent>(OwnerActor);
	AbilitySystemComp->RegisterComponent();
	AbilitySystemComp->InitAbilityActorInfo(OwnerActor, AvatarActor);

	// 히트스톱은 공격자의 시간만 늦춥니다
	AbilitySystemComp->ApplyHitStop(0.2f, 0.1f);
	TestEqual(TEXT("Applying hit stop slows the avatar"), AvatarActor->CustomTimeDilation, 0.1f);

	// 복원은 원래 속도로 정확히 되돌려야 하며, 이 값이 남으면 공격자가 영구히 느려집니다
	AbilitySystemComp->FinishHitStopForTest();
	TestEqual(TEXT("Finishing hit stop restores normal speed"), AvatarActor->CustomTimeDilation, 1.0f);

	// 2타가 이어지면 새 값으로 덮어쓰고, 남은 시간도 새 지속 시간으로 다시 시작해야 합니다
	// 이전 타격의 타이머가 살아 있으면 아직 진행 중인 히트스톱을 먼저 풀어버립니다
	AbilitySystemComp->ApplyHitStop(1.0f, 0.5f);
	AbilitySystemComp->ApplyHitStop(0.4f, 0.2f);
	TestEqual(TEXT("Re-applying hit stop uses the new dilation"), AvatarActor->CustomTimeDilation, 0.2f);
	TestEqual(TEXT("Re-applying hit stop restarts the remaining time"), AbilitySystemComp->GetHitStopRemainingForTest(), 0.4f);

	// 재적용 뒤에도 복원은 한 번이면 충분해야 합니다
	AbilitySystemComp->FinishHitStopForTest();
	TestEqual(TEXT("One restore is enough after re-applying"), AvatarActor->CustomTimeDilation, 1.0f);
	TestFalse(TEXT("No restore remains pending"), AbilitySystemComp->IsHitStopActiveForTest());

	// 보스처럼 히트스톱을 쓰지 않는 공격은 값을 비워두므로 시간에 손대지 않아야 합니다
	AbilitySystemComp->ApplyHitStop(0.0f, 0.1f);
	TestEqual(TEXT("Zero duration does not touch time"), AvatarActor->CustomTimeDilation, 1.0f);
	TestFalse(TEXT("Zero duration schedules no restore"), AbilitySystemComp->IsHitStopActiveForTest());

	// 배율 0은 복원이 실패하면 공격자를 영구히 멈추므로 적용하지 않습니다
	AbilitySystemComp->ApplyHitStop(0.2f, 0.0f);
	TestEqual(TEXT("Zero dilation does not touch time"), AvatarActor->CustomTimeDilation, 1.0f);
	TestFalse(TEXT("Zero dilation schedules no restore"), AbilitySystemComp->IsHitStopActiveForTest());

	// 히트스톱이 걸린 채로 공격자가 사라지면 복원은 되돌릴 대상 없이 예약만 비우고 끝나야 합니다
	AbilitySystemComp->ApplyHitStop(0.2f, 0.1f);
	AvatarActor->Destroy();
	AbilitySystemComp->FinishHitStopForTest();
	TestFalse(TEXT("Restoring after the avatar is gone leaves nothing pending"), AbilitySystemComp->IsHitStopActiveForTest());

	// 공격자가 없으면 새 히트스톱도 예약하지 않아야 하며, 남으면 되돌릴 대상 없는 타이머가 떠돕니다
	AbilitySystemComp->ApplyHitStop(0.2f, 0.1f);
	TestFalse(TEXT("Hit stop without an avatar schedules no restore"), AbilitySystemComp->IsHitStopActiveForTest());

	TestWorld->RemoveFromRoot();
	GEngine->DestroyWorldContext(TestWorld);
	TestWorld->DestroyWorld(false);

	return true;
}

#endif
