#if WITH_DEV_AUTOMATION_TESTS

// 대미지 면역이 피해를 막았을 때 대상 ASC로 연출 GameplayEvent를 보내는 조건만 검증합니다
// 실제 Niagara와 사운드 재생은 어빌리티와 에셋이 담당하므로 PIE 검증이 맡습니다

#include "Misc/AutomationTest.h"

#include "AbilitySystemComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "RSAbilitySystemComponent.h"
#include "RSDamageImmunityFeedbackTestTypes.h"
#include "RSGameplayTags.h"
#include "RSHealthSet.h"

namespace
{
	/** 면역 연출 테스트용 임시 월드를 만들고 Play 상태로 초기화합니다 */
	UWorld* CreateDamageImmunityTestWorld()
	{
		const FName WorldName = MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("RSDamageImmunityTestWorld"), EUniqueObjectNameOptions::GloballyUnique);
		FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
		UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false, WorldName, GetTransientPackage());
		TestWorld->AddToRoot();
		WorldContext.SetCurrentWorld(TestWorld);
		TestWorld->InitializeActorsForPlay(FURL());
		TestWorld->BeginPlay();

		return TestWorld;
	}

	/** 테스트 월드가 남긴 WorldContext와 Root 참조를 함께 정리합니다 */
	void DestroyDamageImmunityTestWorld(UWorld* TestWorld)
	{
		if (!TestWorld)
		{
			return;
		}

		TestWorld->RemoveFromRoot();
		GEngine->DestroyWorldContext(TestWorld);
		TestWorld->DestroyWorld(false);
	}

	/** 실제 공격과 같은 Instant Damage GameplayEffect를 대상 ASC에 적용합니다 */
	void ApplyTestDamage(UAbilitySystemComponent* TargetAbilitySystemComponent, float DamageAmount)
	{
		UGameplayEffect* DamageEffect = NewObject<UGameplayEffect>(GetTransientPackage());
		DamageEffect->DurationPolicy = EGameplayEffectDurationType::Instant;

		FGameplayModifierInfo DamageModifier;
		DamageModifier.Attribute = URSHealthSet::GetDamageAttribute();
		DamageModifier.ModifierOp = EGameplayModOp::Additive;
		DamageModifier.ModifierMagnitude = FScalableFloat(DamageAmount);
		DamageEffect->Modifiers.Add(DamageModifier);

		FGameplayEffectContextHandle EffectContext = TargetAbilitySystemComponent->MakeEffectContext();
		FGameplayEffectSpec DamageSpec(DamageEffect, EffectContext, 1.0f);
		TargetAbilitySystemComponent->ApplyGameplayEffectSpecToSelf(DamageSpec);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSDamageImmunityFeedbackTest, "RS.Combat.DamageImmunity.FeedbackEvent", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSDamageImmunityFeedbackTest::RunTest(const FString& Parameters)
{
	UWorld* TestWorld = CreateDamageImmunityTestWorld();
	TestNotNull(TEXT("Damage immunity test world"), TestWorld);
	if (!TestWorld)
	{
		return false;
	}

	AActor* Owner = TestWorld->SpawnActor<AActor>();
	URSAbilitySystemComponent* AbilitySystemComp = NewObject<URSAbilitySystemComponent>(Owner);
	AbilitySystemComp->RegisterComponent();
	AbilitySystemComp->InitAbilityActorInfo(Owner, Owner);
	// AttributeSet은 소유 Actor를 Outer로 찾으므로 실제 Actor와 같이 Owner를 Outer로 만듭니다
	AbilitySystemComp->AddAttributeSetSubobject(NewObject<URSHealthSet>(Owner));
	AbilitySystemComp->SetNumericAttributeBase(URSHealthSet::GetMaxHealthAttribute(), 100.0f);
	AbilitySystemComp->SetNumericAttributeBase(URSHealthSet::GetHealthAttribute(), 100.0f);

	// 어빌리티를 부여하지 않고 ASC의 public 이벤트 콜백만 구독해 전송 조건 자체를 관찰합니다
	int32 FeedbackEventCount = 0;
	AbilitySystemComp->GenericGameplayEventCallbacks.FindOrAdd(RSGameplayTags::GameplayEvent_Presentation_DamageImmunity).AddLambda(
		[&FeedbackEventCount](const FGameplayEventData* Payload)
		{
			++FeedbackEventCount;
		});

	// 면역이 없는 피해는 체력을 줄이고 연출을 요청하지 않습니다
	ApplyTestDamage(AbilitySystemComp, 10.0f);
	TestEqual(TEXT("Damage without immunity reduces health"), AbilitySystemComp->GetNumericAttribute(URSHealthSet::GetHealthAttribute()), 90.0f);
	TestEqual(TEXT("Damage without immunity requests no feedback"), FeedbackEventCount, 0);

	// 면역이 막은 피해는 체력을 유지하고 연출을 한 번 요청합니다
	AbilitySystemComp->AddLooseGameplayTag(RSGameplayTags::State_Immunity_Damage);
	ApplyTestDamage(AbilitySystemComp, 10.0f);
	TestEqual(TEXT("Damage immunity keeps health"), AbilitySystemComp->GetNumericAttribute(URSHealthSet::GetHealthAttribute()), 90.0f);
	TestEqual(TEXT("Damage immunity requests feedback once"), FeedbackEventCount, 1);

	// 막을 피해가 없으면 면역 상태여도 연출을 요청하지 않습니다
	ApplyTestDamage(AbilitySystemComp, 0.0f);
	TestEqual(TEXT("Zero damage keeps health"), AbilitySystemComp->GetNumericAttribute(URSHealthSet::GetHealthAttribute()), 90.0f);
	TestEqual(TEXT("Zero damage requests no feedback"), FeedbackEventCount, 1);

	// 막은 타격마다 따로 요청하며 연타 제한은 연출 어빌리티의 쿨다운이 담당합니다
	ApplyTestDamage(AbilitySystemComp, 10.0f);
	TestEqual(TEXT("Each blocked hit requests feedback"), FeedbackEventCount, 2);

	AbilitySystemComp->RemoveLooseGameplayTag(RSGameplayTags::State_Immunity_Damage);

	DestroyDamageImmunityTestWorld(TestWorld);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSDamageImmunityFeedbackAbilityTest, "RS.Combat.DamageImmunity.FeedbackAbility", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSDamageImmunityFeedbackAbilityTest::RunTest(const FString& Parameters)
{
	UWorld* TestWorld = CreateDamageImmunityTestWorld();
	TestNotNull(TEXT("Damage immunity test world"), TestWorld);
	if (!TestWorld)
	{
		return false;
	}

	AActor* Owner = TestWorld->SpawnActor<AActor>();
	URSAbilitySystemComponent* AbilitySystemComp = NewObject<URSAbilitySystemComponent>(Owner);
	AbilitySystemComp->RegisterComponent();
	AbilitySystemComp->InitAbilityActorInfo(Owner, Owner);
	AbilitySystemComp->AddAttributeSetSubobject(NewObject<URSHealthSet>(Owner));
	AbilitySystemComp->SetNumericAttributeBase(URSHealthSet::GetMaxHealthAttribute(), 100.0f);
	AbilitySystemComp->SetNumericAttributeBase(URSHealthSet::GetHealthAttribute(), 100.0f);
	AbilitySystemComp->GiveAbility(FGameplayAbilitySpec(URSDamageImmunityFeedbackTestAbility::StaticClass(), 1));

	// HealthSet이 보내는 태그와 어빌리티가 등록한 트리거 태그가 어긋나면 연출이 조용히 사라지므로 활성화 자체를 관찰합니다
	int32 ActivatedCount = 0;
	AbilitySystemComp->AbilityActivatedCallbacks.AddLambda(
		[&ActivatedCount](UGameplayAbility* ActivatedAbility)
		{
			if (ActivatedAbility && ActivatedAbility->IsA<URSGameplayAbility_DamageImmunityFeedback>())
			{
				++ActivatedCount;
			}
		});

	// 면역이 없는 피해는 연출 어빌리티를 활성화하지 않습니다
	ApplyTestDamage(AbilitySystemComp, 10.0f);
	TestEqual(TEXT("Damage without immunity activates no feedback ability"), ActivatedCount, 0);

	// 면역이 막은 피해는 연출 어빌리티를 활성화합니다
	AbilitySystemComp->AddLooseGameplayTag(RSGameplayTags::State_Immunity_Damage);
	ApplyTestDamage(AbilitySystemComp, 10.0f);
	TestEqual(TEXT("Damage immunity activates the feedback ability"), ActivatedCount, 1);

	// 쿨다운이 도는 동안 이어지는 다단 히트는 같은 연출을 겹쳐 재생하지 않습니다
	ApplyTestDamage(AbilitySystemComp, 10.0f);
	TestTrue(TEXT("The feedback ability applies its cooldown"), AbilitySystemComp->HasMatchingGameplayTag(RSGameplayTags::Cooldown_Ability_DamageImmunityFeedback));
	TestEqual(TEXT("A blocked hit during the cooldown repeats no feedback"), ActivatedCount, 1);

	// 쿨다운이 끝나면 다음 타격의 연출이 다시 나야 하므로 어빌리티가 활성 상태를 남기지 않았는지 확인합니다
	AbilitySystemComp->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(RSGameplayTags::Cooldown_Ability_DamageImmunityFeedback));
	ApplyTestDamage(AbilitySystemComp, 10.0f);
	TestEqual(TEXT("A blocked hit after the cooldown activates the feedback ability again"), ActivatedCount, 2);

	AbilitySystemComp->RemoveLooseGameplayTag(RSGameplayTags::State_Immunity_Damage);

	DestroyDamageImmunityTestWorld(TestWorld);

	return true;
}

#endif
