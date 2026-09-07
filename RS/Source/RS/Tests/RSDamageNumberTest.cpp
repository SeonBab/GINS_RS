#if WITH_DEV_AUTOMATION_TESTS

// URSDamageNumberViewModel의 값 변환과 구독 생명주기만 검증합니다
// 투영, 위젯 풀, 화면 배치는 위젯 트리와 카메라가 필요하므로 PIE 검증이 담당합니다
// public lifecycle 계약과 ASC의 public 델리게이트만 사용하므로 생산 코드에 테스트용 접근을 열지 않습니다

#include "Misc/AutomationTest.h"
#include "RSAbilitySystemComponent.h"
#include "RSDamageNumberTestTypes.h"
#include "RSDamageNumberViewModel.h"

namespace RSDamageNumberTest
{
	// 테스트마다 새 객체를 사용해 이전 구독이 결과에 섞이지 않게 합니다
	struct FFixture
	{
		FFixture()
			: ViewModel(NewObject<URSDamageNumberViewModel>())
			, AbilitySystemComp(NewObject<URSAbilitySystemComponent>())
			, Listener(NewObject<URSDamageNumberTestListener>())
		{
			Listener->ListenTo(ViewModel);
		}

		URSDamageNumberViewModel* ViewModel = nullptr;
		URSAbilitySystemComponent* AbilitySystemComp = nullptr;
		URSDamageNumberTestListener* Listener = nullptr;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSDamageNumberTest, "RS.UI.DamageNumber", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSDamageNumberTest::RunTest(const FString& Parameters)
{
	const FVector TargetLocation(100.0f, -200.0f, 50.0f);

	// 정수 데미지 invariant 위에서 가장 가까운 정수로 표시하며, 양수 피해는 0으로 표시하지 않습니다
	{
		const float AppliedDamages[] = { 10.0f, 10.4f, 10.6f, 0.4f, 1.0f };
		const TCHAR* ExpectedTexts[] = { TEXT("10"), TEXT("10"), TEXT("11"), TEXT("1"), TEXT("1") };

		for (int32 Index = 0; Index < UE_ARRAY_COUNT(AppliedDamages); ++Index)
		{
			RSDamageNumberTest::FFixture Fixture;
			Fixture.ViewModel->InitializeViewModel(Fixture.AbilitySystemComp);
			Fixture.AbilitySystemComp->OnDamageDealt.Broadcast(AppliedDamages[Index], TargetLocation);

			const FString Context = FString::Printf(TEXT("Display %.1f"), AppliedDamages[Index]);
			if (TestEqual(Context, Fixture.Listener->RequestedTexts.Num(), 1))
			{
				TestEqual(Context, Fixture.Listener->RequestedTexts[0], FString(ExpectedTexts[Index]));
			}
		}
	}

	// 자릿수가 늘어도 구분 기호를 넣지 않습니다
	{
		RSDamageNumberTest::FFixture Fixture;
		Fixture.ViewModel->InitializeViewModel(Fixture.AbilitySystemComp);
		Fixture.AbilitySystemComp->OnDamageDealt.Broadcast(12345.0f, TargetLocation);

		if (TestEqual(TEXT("No grouping"), Fixture.Listener->RequestedTexts.Num(), 1))
		{
			TestEqual(TEXT("No grouping"), Fixture.Listener->RequestedTexts[0], FString(TEXT("12345")));
		}
	}

	// 하한 1이 음수를 1로 만들지 않아야 합니다
	{
		const float NonPositiveDamages[] = { 0.0f, -0.4f, -10.0f };
		for (float NonPositiveDamage : NonPositiveDamages)
		{
			RSDamageNumberTest::FFixture Fixture;
			Fixture.ViewModel->InitializeViewModel(Fixture.AbilitySystemComp);
			Fixture.AbilitySystemComp->OnDamageDealt.Broadcast(NonPositiveDamage, TargetLocation);

			const FString Context = FString::Printf(TEXT("Non positive %.1f"), NonPositiveDamage);
			TestEqual(Context, Fixture.Listener->RequestedTexts.Num(), 0);
		}
	}

	// 앵커는 전달된 원좌표를 수직으로만 옮기며, 옮기는 양은 위치와 무관합니다
	{
		RSDamageNumberTest::FFixture Fixture;
		Fixture.ViewModel->InitializeViewModel(Fixture.AbilitySystemComp);

		const FVector SecondLocation(-40.0f, 15.0f, 900.0f);
		Fixture.AbilitySystemComp->OnDamageDealt.Broadcast(10.0f, TargetLocation);
		Fixture.AbilitySystemComp->OnDamageDealt.Broadcast(10.0f, SecondLocation);

		if (TestEqual(TEXT("Anchor count"), Fixture.Listener->RequestedAnchors.Num(), 2))
		{
			const FVector FirstAnchor = Fixture.Listener->RequestedAnchors[0];
			const FVector SecondAnchor = Fixture.Listener->RequestedAnchors[1];

			TestEqual(TEXT("Anchor X"), FirstAnchor.X, TargetLocation.X);
			TestEqual(TEXT("Anchor Y"), FirstAnchor.Y, TargetLocation.Y);
			TestTrue(TEXT("Anchor is raised"), FirstAnchor.Z > TargetLocation.Z);

			// 표시 높이는 조정용 값이므로 특정 수치를 고정하지 않고 좌표에 무관하다는 계약만 검사합니다
			TestEqual(TEXT("Anchor offset is translation invariant"), SecondAnchor - SecondLocation, FirstAnchor - TargetLocation);
		}
	}

	// 같은 값이 연속으로 들어와도 요청은 매번 발생해야 합니다
	{
		RSDamageNumberTest::FFixture Fixture;
		Fixture.ViewModel->InitializeViewModel(Fixture.AbilitySystemComp);

		for (int32 RepeatIndex = 0; RepeatIndex < 3; ++RepeatIndex)
		{
			Fixture.AbilitySystemComp->OnDamageDealt.Broadcast(37.0f, TargetLocation);
		}

		TestEqual(TEXT("Repeated same damage"), Fixture.Listener->RequestedTexts.Num(), 3);
	}

	// 같은 ASC 재초기화는 배선을 유지하며 표시 중인 숫자를 폐기하지 않습니다
	{
		RSDamageNumberTest::FFixture Fixture;
		Fixture.ViewModel->InitializeViewModel(Fixture.AbilitySystemComp);
		Fixture.ViewModel->InitializeViewModel(Fixture.AbilitySystemComp);
		Fixture.AbilitySystemComp->OnDamageDealt.Broadcast(10.0f, TargetLocation);

		TestEqual(TEXT("Reinitialize request count"), Fixture.Listener->RequestedTexts.Num(), 1);
		TestEqual(TEXT("Reinitialize reset count"), Fixture.Listener->ResetCount, 0);
	}

	// nullptr은 해제가 아니라 아무 작업도 하지 않는다는 계약입니다
	{
		RSDamageNumberTest::FFixture Fixture;
		Fixture.ViewModel->InitializeViewModel(Fixture.AbilitySystemComp);
		Fixture.ViewModel->InitializeViewModel(nullptr);
		Fixture.AbilitySystemComp->OnDamageDealt.Broadcast(10.0f, TargetLocation);

		TestEqual(TEXT("Null reinitialize request count"), Fixture.Listener->RequestedTexts.Num(), 1);
		TestEqual(TEXT("Null reinitialize reset count"), Fixture.Listener->ResetCount, 0);
	}

	// 해제는 폐기를 한 번 요청하고 이후 이벤트를 받지 않습니다
	{
		RSDamageNumberTest::FFixture Fixture;
		Fixture.ViewModel->InitializeViewModel(Fixture.AbilitySystemComp);
		Fixture.ViewModel->UninitializeViewModel();
		Fixture.AbilitySystemComp->OnDamageDealt.Broadcast(10.0f, TargetLocation);

		TestEqual(TEXT("Uninitialize reset count"), Fixture.Listener->ResetCount, 1);
		TestEqual(TEXT("Uninitialize request count"), Fixture.Listener->RequestedTexts.Num(), 0);
	}

	// 해제는 멱등이므로 두 번 호출해도 폐기는 한 번입니다
	{
		RSDamageNumberTest::FFixture Fixture;
		Fixture.ViewModel->InitializeViewModel(Fixture.AbilitySystemComp);
		Fixture.ViewModel->UninitializeViewModel();
		Fixture.ViewModel->UninitializeViewModel();

		TestEqual(TEXT("Idempotent uninitialize"), Fixture.Listener->ResetCount, 1);
	}

	// 연결하지 않은 상태의 해제는 아무 일도 하지 않습니다
	{
		RSDamageNumberTest::FFixture Fixture;
		Fixture.ViewModel->UninitializeViewModel();

		TestEqual(TEXT("Uninitialize without source"), Fixture.Listener->ResetCount, 0);
	}

	// ASC 교체는 이전 연결을 끊고 폐기를 요청한 뒤 새 원본만 관찰합니다
	{
		RSDamageNumberTest::FFixture Fixture;
		URSAbilitySystemComponent* SecondAbilitySystemComp = NewObject<URSAbilitySystemComponent>();

		Fixture.ViewModel->InitializeViewModel(Fixture.AbilitySystemComp);
		Fixture.ViewModel->InitializeViewModel(SecondAbilitySystemComp);

		TestEqual(TEXT("Swap reset count"), Fixture.Listener->ResetCount, 1);

		Fixture.AbilitySystemComp->OnDamageDealt.Broadcast(10.0f, TargetLocation);
		TestEqual(TEXT("Swap ignores previous source"), Fixture.Listener->RequestedTexts.Num(), 0);

		SecondAbilitySystemComp->OnDamageDealt.Broadcast(20.0f, TargetLocation);
		if (TestEqual(TEXT("Swap observes new source"), Fixture.Listener->RequestedTexts.Num(), 1))
		{
			TestEqual(TEXT("Swap observes new source"), Fixture.Listener->RequestedTexts[0], FString(TEXT("20")));
		}
	}

	return true;
}

#endif
