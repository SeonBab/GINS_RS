// Fill out your copyright notice in the Description page of Project Settings.


#include "RSPlayerAbilityViewModel.h"

#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "RSAbilityDefinition.h"
#include "RSAbilitySlotViewModel.h"
#include "RSAbilitySystemComponent.h"
#include "RSBaseGameplayAbility.h"
#include "RSInputConfig.h"
#include "RSPlayerCharacter.h"

void URSPlayerAbilityViewModel::BeginDestroy()
{
	UnbindFromSource();

	Super::BeginDestroy();
}

void URSPlayerAbilityViewModel::HandleSourceRegistered(UObject* Source)
{
	ARSPlayerCharacter* InPlayerCharacter = Cast<ARSPlayerCharacter>(Source);
	if (!IsValid(InPlayerCharacter))
	{
		return;
	}

	BindToSource(InPlayerCharacter);
}

void URSPlayerAbilityViewModel::HandleSourceUnregistered(UObject* Source)
{
	if (PlayerCharacter.Get() != Source)
	{
		return;
	}

	UnbindFromSource();
}

URSAbilitySlotViewModel* URSPlayerAbilityViewModel::GetOrCreateSlotViewModel(FGameplayTag InputTag, UMaterialInterface* IconMaterial)
{
	if (!InputTag.IsValid())
	{
		return nullptr;
	}

	if (FRSAbilitySlotBinding* ExistingBinding = SlotBindings.Find(InputTag))
	{
		return ExistingBinding->ViewModel;
	}

	FRSAbilitySlotBinding NewBinding;
	NewBinding.ViewModel = NewObject<URSAbilitySlotViewModel>(this);
	NewBinding.ViewModel->SetInputTag(InputTag);

	// 아래 RefreshSlot이 아이콘 Brush를 만들기 전에 지정해야 머티리얼이 반영됩니다
	NewBinding.ViewModel->SetIconMaterial(IconMaterial);

	FRSAbilitySlotBinding& AddedBinding = SlotBindings.Add(InputTag, MoveTemp(NewBinding));

	// 슬롯이 뒤늦게 추가되어도 현재 상태를 즉시 반영합니다
	RefreshSlot(InputTag, AddedBinding);

	return AddedBinding.ViewModel;
}

void URSPlayerAbilityViewModel::RefreshSlots()
{
	for (TPair<FGameplayTag, FRSAbilitySlotBinding>& SlotPair : SlotBindings)
	{
		RefreshSlot(SlotPair.Key, SlotPair.Value);
	}
}

void URSPlayerAbilityViewModel::BindToSource(ARSPlayerCharacter* InPlayerCharacter)
{
	URSAbilitySystemComponent* NewAbilitySystemComp = Cast<URSAbilitySystemComponent>(InPlayerCharacter->GetAbilitySystemComponent());

	// 같은 원본을 다시 알려온 경우에는 구독을 다시 만들지 않고 현재 상태만 반영합니다
	if (PlayerCharacter.Get() == InPlayerCharacter && AbilitySystemComp.Get() == NewAbilitySystemComp)
	{
		RefreshSlots();

		return;
	}

	UnbindFromSource();

	PlayerCharacter = InPlayerCharacter;

	// Pawn 소유 시점에는 PlayerState와 ASC가 아직 없을 수 있으며, 준비되면 같은 경로로 다시 알려옵니다
	if (!NewAbilitySystemComp)
	{
		return;
	}

	AbilitySystemComp = NewAbilitySystemComp;

	NewAbilitySystemComp->OnGrantedAbilitiesChanged.AddUniqueDynamic(this, &ThisClass::HandleGrantedAbilitiesChanged);

	// MappingContext 추가는 Pawn 소유보다 늦게 일어나므로, 지금 키를 조회하면 아직 비어 있습니다
	// 매핑이 구성된 뒤 알림을 받아야 슬롯에 키가 표시됩니다
	if (const APlayerController* PlayerController = InPlayerCharacter->GetController<APlayerController>())
	{
		if (const ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				EnhancedInputSubsystem = InputSubsystem;
				InputSubsystem->ControlMappingsRebuiltDelegate.AddUniqueDynamic(this, &ThisClass::HandleControlMappingsRebuilt);
			}
		}
	}

	// 슬롯이 대표하는 어빌리티를 바꾸는 상태만 구독합니다
	// 행동 잠금처럼 실행 가능 여부만 바꾸는 상태까지 구독하면 표시가 바뀌지 않는데도 매 행동마다 다시 계산합니다
	for (const FGameplayTag& DisplayContextTag : URSAbilitySystemComponent::GetDisplayContextTags())
	{
		const FDelegateHandle DelegateHandle = NewAbilitySystemComp->RegisterGameplayTagEvent(DisplayContextTag, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::HandleDisplayContextTagChanged);

		DisplayContextTags.Add(DisplayContextTag);
		DisplayContextTagDelegateHandles.Add(DelegateHandle);
	}

	RefreshSlots();
}

void URSPlayerAbilityViewModel::UnbindFromSource()
{
	if (URSAbilitySystemComponent* CurrentAbilitySystemComp = AbilitySystemComp.Get())
	{
		CurrentAbilitySystemComp->OnGrantedAbilitiesChanged.RemoveDynamic(this, &ThisClass::HandleGrantedAbilitiesChanged);

		for (int32 TagIndex = 0; TagIndex < DisplayContextTags.Num(); ++TagIndex)
		{
			CurrentAbilitySystemComp->UnregisterGameplayTagEvent(DisplayContextTagDelegateHandles[TagIndex], DisplayContextTags[TagIndex], EGameplayTagEventType::NewOrRemoved);
		}
	}

	if (UEnhancedInputLocalPlayerSubsystem* CurrentInputSubsystem = EnhancedInputSubsystem.Get())
	{
		CurrentInputSubsystem->ControlMappingsRebuiltDelegate.RemoveDynamic(this, &ThisClass::HandleControlMappingsRebuilt);
	}

	EnhancedInputSubsystem.Reset();

	DisplayContextTags.Reset();
	DisplayContextTagDelegateHandles.Reset();

	for (TPair<FGameplayTag, FRSAbilitySlotBinding>& SlotPair : SlotBindings)
	{
		ClearCooldownSubscription(SlotPair.Value);

		SlotPair.Value.ResolvedAbilityHandle = FGameplayAbilitySpecHandle();

		if (URSAbilitySlotViewModel* SlotViewModel = SlotPair.Value.ViewModel)
		{
			SlotViewModel->SetPresentation(nullptr, nullptr);
			SlotViewModel->ClearCooldown();
		}
	}

	AbilitySystemComp.Reset();
	PlayerCharacter.Reset();
}

void URSPlayerAbilityViewModel::RefreshSlot(const FGameplayTag& InputTag, FRSAbilitySlotBinding& SlotBinding)
{
	URSAbilitySlotViewModel* SlotViewModel = SlotBinding.ViewModel;
	if (!SlotViewModel)
	{
		return;
	}

	SlotViewModel->SetInputKeyText(GetInputKeyTextForInputTag(InputTag));

	const URSAbilitySystemComponent* CurrentAbilitySystemComp = AbilitySystemComp.Get();
	if (!CurrentAbilitySystemComp)
	{
		ClearCooldownSubscription(SlotBinding);
		SlotBinding.ResolvedAbilityHandle = FGameplayAbilitySpecHandle();
		SlotViewModel->SetPresentation(nullptr, nullptr);
		SlotViewModel->ClearCooldown();

		return;
	}

	// 슬롯이 무엇을 뜻하는지는 게임플레이 계층이 정하며 여기서는 상태를 해석하지 않습니다
	const FRSAbilityDisplayResolveResult ResolveResult = CurrentAbilitySystemComp->ResolveDisplayAbilityForInputTag(InputTag);

	const URSBaseGameplayAbility* ResolvedAbility = nullptr;
	if (ResolveResult.Status == ERSAbilityDisplayResolveStatus::Resolved)
	{
		if (const FGameplayAbilitySpec* AbilitySpec = CurrentAbilitySystemComp->FindAbilitySpecFromHandle(ResolveResult.AbilityHandle))
		{
			ResolvedAbility = Cast<URSBaseGameplayAbility>(AbilitySpec->Ability);
		}
	}

	SlotBinding.ResolvedAbilityHandle = ResolvedAbility ? ResolveResult.AbilityHandle : FGameplayAbilitySpecHandle();

	if (!ResolvedAbility)
	{
		ClearCooldownSubscription(SlotBinding);
		SlotViewModel->SetPresentation(nullptr, nullptr);
		SlotViewModel->ClearCooldown();

		return;
	}

	const URSAbilityDefinition* AbilityDefinition = ResolvedAbility->GetAbilityDefinition();
	UTexture2D* IconTexture = AbilityDefinition ? LoadAbilityIcon(AbilityDefinition->Icon) : nullptr;
	SlotViewModel->SetPresentation(AbilityDefinition, IconTexture);

	const FGameplayTagContainer* CooldownTags = ResolvedAbility->GetCooldownTags();
	UpdateCooldownSubscription(SlotBinding, CooldownTags ? *CooldownTags : FGameplayTagContainer());

	UpdateSlotCooldown(SlotBinding);
}

void URSPlayerAbilityViewModel::UpdateSlotCooldown(const FRSAbilitySlotBinding& SlotBinding) const
{
	URSAbilitySlotViewModel* SlotViewModel = SlotBinding.ViewModel;
	const URSAbilitySystemComponent* CurrentAbilitySystemComp = AbilitySystemComp.Get();
	if (!SlotViewModel || !CurrentAbilitySystemComp)
	{
		return;
	}

	float Remaining = 0.0f;
	float Duration = 0.0f;
	if (!CurrentAbilitySystemComp->GetCooldownInfoForAbility(SlotBinding.ResolvedAbilityHandle, Remaining, Duration))
	{
		SlotViewModel->ClearCooldown();

		return;
	}

	const UWorld* World = CurrentAbilitySystemComp->GetWorld();
	const float CurrentWorldTime = World ? World->GetTimeSeconds() : 0.0f;

	// 남은 시간이 아니라 만료 시각을 전달하여 위젯이 진행률을 스스로 계산하고 매 프레임 통지가 생기지 않게 합니다
	SlotViewModel->SetCooldown(CurrentWorldTime + Remaining, Duration);
}

void URSPlayerAbilityViewModel::UpdateCooldownSubscription(FRSAbilitySlotBinding& SlotBinding, const FGameplayTagContainer& NewCooldownTags)
{
	URSAbilitySystemComponent* CurrentAbilitySystemComp = AbilitySystemComp.Get();
	if (!CurrentAbilitySystemComp)
	{
		ClearCooldownSubscription(SlotBinding);

		return;
	}

	// 대표 어빌리티가 그대로면 같은 구독을 유지하여 불필요한 재등록을 만들지 않습니다
	if (SlotBinding.CooldownTags.Num() == NewCooldownTags.Num())
	{
		bool bIsSameSubscription = true;
		for (const FGameplayTag& CooldownTag : NewCooldownTags)
		{
			if (!SlotBinding.CooldownTags.Contains(CooldownTag))
			{
				bIsSameSubscription = false;
				break;
			}
		}

		if (bIsSameSubscription)
		{
			return;
		}
	}

	ClearCooldownSubscription(SlotBinding);

	for (const FGameplayTag& CooldownTag : NewCooldownTags)
	{
		const FDelegateHandle DelegateHandle = CurrentAbilitySystemComp->RegisterGameplayTagEvent(CooldownTag, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::HandleCooldownTagChanged);

		SlotBinding.CooldownTags.Add(CooldownTag);
		SlotBinding.CooldownTagDelegateHandles.Add(DelegateHandle);
	}
}

void URSPlayerAbilityViewModel::ClearCooldownSubscription(FRSAbilitySlotBinding& SlotBinding)
{
	if (URSAbilitySystemComponent* CurrentAbilitySystemComp = AbilitySystemComp.Get())
	{
		for (int32 TagIndex = 0; TagIndex < SlotBinding.CooldownTags.Num(); ++TagIndex)
		{
			CurrentAbilitySystemComp->UnregisterGameplayTagEvent(SlotBinding.CooldownTagDelegateHandles[TagIndex], SlotBinding.CooldownTags[TagIndex], EGameplayTagEventType::NewOrRemoved);
		}
	}

	SlotBinding.CooldownTags.Reset();
	SlotBinding.CooldownTagDelegateHandles.Reset();
}

FText URSPlayerAbilityViewModel::GetInputKeyTextForInputTag(const FGameplayTag& InputTag) const
{
	const ARSPlayerCharacter* CurrentPlayerCharacter = PlayerCharacter.Get();
	if (!CurrentPlayerCharacter)
	{
		return FText::GetEmpty();
	}

	const URSInputConfig* InputConfig = CurrentPlayerCharacter->GetInputConfig();
	const UEnhancedInputLocalPlayerSubsystem* CurrentInputSubsystem = EnhancedInputSubsystem.Get();
	if (!InputConfig || !CurrentInputSubsystem)
	{
		return FText::GetEmpty();
	}

	const UInputAction* InputAction = InputConfig->FindAbilityInputAction(InputTag, false);
	if (!InputAction)
	{
		return FText::GetEmpty();
	}

	// 하나의 InputAction에 여러 장치의 키가 매핑될 수 있으므로 표시할 키를 결정적으로 고릅니다
	// 장치별 아이콘 전환은 아직 다루지 않으므로 키보드와 마우스 키를 우선합니다
	const TArray<FKey> MappedKeys = CurrentInputSubsystem->QueryKeysMappedToAction(InputAction);
	for (const FKey& MappedKey : MappedKeys)
	{
		if (!MappedKey.IsGamepadKey())
		{
			return GetCultureInvariantKeyText(MappedKey);
		}
	}

	return MappedKeys.IsEmpty() ? FText::GetEmpty() : GetCultureInvariantKeyText(MappedKeys[0]);
}

FText URSPlayerAbilityViewModel::GetCultureInvariantKeyText(const FKey& Key)
{
	const FText DisplayName = Key.GetDisplayName();

	// 엔진이 키 이름을 번역하므로 게임 언어에 따라 표기가 달라집니다
	// 슬롯의 키 표기는 언어와 무관하게 같아야 하므로 번역 전 원문을 사용합니다
	const FString* SourceString = FTextInspector::GetSourceString(DisplayName);
	if (!SourceString || SourceString->IsEmpty())
	{
		return DisplayName;
	}

	// 엔진 원문은 "Space Bar"처럼 단어마다 대문자로 시작하므로 첫 글자만 남기고 낮춥니다
	FString KeyLabel = SourceString->ToLower();
	KeyLabel[0] = FChar::ToUpper(KeyLabel[0]);

	return FText::AsCultureInvariant(KeyLabel);
}

UTexture2D* URSPlayerAbilityViewModel::LoadAbilityIcon(const TSoftObjectPtr<UTexture2D>& IconPath)
{
	if (IconPath.IsNull())
	{
		return nullptr;
	}

	if (const TObjectPtr<UTexture2D>* CachedIcon = LoadedIcons.Find(IconPath.ToSoftObjectPath()))
	{
		return *CachedIcon;
	}

	// 현재 표시 대상이 몇 개뿐이라 동기 로드로 충분하며, 후보가 늘어나면 이 함수 안에서만 비동기로 바꿉니다
	UTexture2D* IconTexture = IconPath.LoadSynchronous();
	if (!IconTexture)
	{
		return nullptr;
	}

	LoadedIcons.Add(IconPath.ToSoftObjectPath(), IconTexture);

	return IconTexture;
}

void URSPlayerAbilityViewModel::HandleGrantedAbilitiesChanged(URSAbilitySystemComponent* InAbilitySystemComponent)
{
	if (AbilitySystemComp.Get() != InAbilitySystemComponent)
	{
		return;
	}

	RefreshSlots();
}

void URSPlayerAbilityViewModel::HandleDisplayContextTagChanged(const FGameplayTag ChangedTag, int32 NewCount)
{
	RefreshSlots();
}

void URSPlayerAbilityViewModel::HandleControlMappingsRebuilt()
{
	// 표시할 어빌리티는 그대로이고 키 표시만 달라지므로 슬롯을 다시 결정하지 않습니다
	for (const TPair<FGameplayTag, FRSAbilitySlotBinding>& SlotPair : SlotBindings)
	{
		if (URSAbilitySlotViewModel* SlotViewModel = SlotPair.Value.ViewModel)
		{
			SlotViewModel->SetInputKeyText(GetInputKeyTextForInputTag(SlotPair.Key));
		}
	}
}

void URSPlayerAbilityViewModel::HandleCooldownTagChanged(const FGameplayTag ChangedTag, int32 NewCount)
{
	// 어느 슬롯의 쿨다운인지 태그로 특정하여 나머지 슬롯은 다시 계산하지 않습니다
	for (const TPair<FGameplayTag, FRSAbilitySlotBinding>& SlotPair : SlotBindings)
	{
		if (SlotPair.Value.CooldownTags.Contains(ChangedTag))
		{
			UpdateSlotCooldown(SlotPair.Value);
		}
	}
}
