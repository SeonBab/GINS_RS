#include "RSGameplayAbility_InteractionScan.h"

#include "AbilitySystemComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "RSGameplayTags.h"
#include "RSInteractable.h"
#include "RSInteractionPromptWidget.h"
#include "RSPlayerController.h"
#include "Tasks/RSAbilityTask_ScanInteractables.h"

URSGameplayAbility_InteractionScan::URSGameplayAbility_InteractionScan()
{
	ActivationPolicy = ERSAbilityActivationPolicy::OnSpawn;

	// 선택된 대상을 인스턴스가 들고 있으므로 실행 어빌리티가 찾아올 수 있는 Actor 단위 인스턴스가 필요합니다
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(RSGameplayTags::Ability_Interaction_Scan);
	SetAssetTags(AssetTags);
}

URSGameplayAbility_InteractionScan* URSGameplayAbility_InteractionScan::FindInstance(const UAbilitySystemComponent* AbilitySystemComponent)
{
	if (!AbilitySystemComponent)
	{
		return nullptr;
	}

	const FGameplayAbilitySpec* AbilitySpec = AbilitySystemComponent->FindAbilitySpecFromClass(StaticClass());
	if (!AbilitySpec)
	{
		return nullptr;
	}

	return Cast<URSGameplayAbility_InteractionScan>(AbilitySpec->GetPrimaryInstance());
}

void URSGameplayAbility_InteractionScan::SetPromptSuppressed(bool bSuppressed)
{
	bIsPromptSuppressed = bSuppressed;
	UpdatePromptForFocus(FocusedInteractable.Get());
}

void URSGameplayAbility_InteractionScan::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	CreatePromptWidget();

	URSAbilityTask_ScanInteractables* ScanTask = URSAbilityTask_ScanInteractables::ScanInteractables(this, ScanRadius, ScanInterval, ScanChannel);
	if (!ScanTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);

		return;
	}

	ScanTask->OnFocusChanged.AddDynamic(this, &ThisClass::HandleFocusChanged);
	ScanTask->ReadyForActivation();
}

void URSGameplayAbility_InteractionScan::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	ClearGrantedInteractAbility();
	FocusedInteractable.Reset();
	NotifyControllerOfFocusChange(nullptr);

	if (PromptWidgetComp)
	{
		PromptWidgetComp->DestroyComponent();
		PromptWidgetComp = nullptr;
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void URSGameplayAbility_InteractionScan::HandleFocusChanged(AActor* FocusedActor)
{
	FocusedInteractable = FocusedActor;
	NotifyControllerOfFocusChange(FocusedActor);

	UpdateGrantedAbilityForFocus(FocusedActor);
	UpdatePromptForFocus(FocusedActor);
}

void URSGameplayAbility_InteractionScan::CreatePromptWidget()
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!IsValid(AvatarActor) || !PromptWidgetClass)
	{
		return;
	}

	PromptWidgetComp = NewObject<UWidgetComponent>(AvatarActor);
	PromptWidgetComp->SetWidgetClass(PromptWidgetClass);

	// 월드 공간 위젯은 카메라 거리에 따라 글자가 뭉개지므로 대상 위치에 붙되 화면 공간으로 그립니다
	PromptWidgetComp->SetWidgetSpace(EWidgetSpace::Screen);
	PromptWidgetComp->SetDrawAtDesiredSize(true);
	PromptWidgetComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PromptWidgetComp->SetVisibility(false);
	PromptWidgetComp->SetupAttachment(AvatarActor->GetRootComponent());
	PromptWidgetComp->RegisterComponent();
}

void URSGameplayAbility_InteractionScan::UpdatePromptForFocus(AActor* FocusedActor)
{
	if (!PromptWidgetComp)
	{
		return;
	}

	if (bIsPromptSuppressed || !IsValid(FocusedActor))
	{
		PromptWidgetComp->SetVisibility(false);

		return;
	}

	USceneComponent* PromptAnchor = IRSInteractable::Execute_GetPromptAnchor(FocusedActor);
	if (!PromptAnchor)
	{
		PromptAnchor = FocusedActor->GetRootComponent();
	}

	if (!PromptAnchor)
	{
		PromptWidgetComp->SetVisibility(false);

		return;
	}

	// 대상에 붙여 두면 대상이 움직여도 Tick 없이 따라갑니다
	PromptWidgetComp->AttachToComponent(PromptAnchor, FAttachmentTransformRules::SnapToTargetNotIncludingScale);

	if (URSInteractionPromptWidget* PromptWidget = Cast<URSInteractionPromptWidget>(PromptWidgetComp->GetUserWidgetObject()))
	{
		PromptWidget->SetPromptText(IRSInteractable::Execute_GetPromptText(FocusedActor));
	}

	PromptWidgetComp->SetVisibility(true);
}

void URSGameplayAbility_InteractionScan::NotifyControllerOfFocusChange(AActor* FocusedActor) const
{
	const APawn* AvatarPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	ARSPlayerController* PlayerController = AvatarPawn ? Cast<ARSPlayerController>(AvatarPawn->GetController()) : nullptr;
	if (PlayerController)
	{
		PlayerController->HandleInteractionFocusChanged(FocusedActor);
	}
}

void URSGameplayAbility_InteractionScan::UpdateGrantedAbilityForFocus(AActor* FocusedActor)
{
	ClearGrantedInteractAbility();

	if (!IsValid(FocusedActor))
	{
		return;
	}

	const TSubclassOf<URSBaseGameplayAbility> InteractAbilityClass = IRSInteractable::Execute_GetInteractAbilityClass(FocusedActor);
	if (!InteractAbilityClass)
	{
		return;
	}

	UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo();
	if (!AbilitySystemComponent || !AbilitySystemComponent->IsOwnerActorAuthoritative())
	{
		return;
	}

	// 부여 수명을 선택 상태와 일치시키면 대상이 바뀔 때마다 회수되므로 캐시를 따로 두지 않습니다
	FGameplayAbilitySpec AbilitySpec(InteractAbilityClass, 1);
	AbilitySpec.SourceObject = FocusedActor;

	GrantedInteractAbilityHandle = AbilitySystemComponent->GiveAbility(AbilitySpec);
}

void URSGameplayAbility_InteractionScan::ClearGrantedInteractAbility()
{
	if (!GrantedInteractAbilityHandle.IsValid())
	{
		return;
	}

	if (UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo())
	{
		if (AbilitySystemComponent->IsOwnerActorAuthoritative())
		{
			AbilitySystemComponent->ClearAbility(GrantedInteractAbilityHandle);
		}
	}

	GrantedInteractAbilityHandle = FGameplayAbilitySpecHandle();
}
