// Fill out your copyright notice in the Description page of Project Settings.


#include "RSPlayerCharacter.h"

#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "RSAbilitySystemComponent.h"
#include "RSHealthComponent.h"
#include "RSPlayerController.h"
#include "RSPlayerState.h"
#include "RSGameplayTags.h"
#include "RSInputComponent.h"
#include "RSInputConfig.h"

ARSPlayerCharacter::ARSPlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	SpringArmComp = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArmComponent"));
	SpringArmComp->SetupAttachment(GetRootComponent());

	CameraComp = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));
	CameraComp->SetupAttachment(SpringArmComp);

	HealthComp = CreateDefaultSubobject<URSHealthComponent>(TEXT("HealthComponent"));

	// 클릭 경로가 꺾일 때 CharacterMovement가 현재 진행 방향을 기준으로 캐릭터를 회전시킵니다
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
}

void ARSPlayerCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	HealthComp->OnDeathStarted.AddUniqueDynamic(this, &ThisClass::HandleDeathStarted);
}

void ARSPlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	InitializeAbilitySystem();
}

// 입력 데이터 에셋을 사용하여 캐릭터의 입력 액션을 바인딩합니다
void ARSPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	APlayerController* PlayerController = Cast<APlayerController>(GetController());

	if (!PlayerController)
	{
		return;
	}

	ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer();

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
	{
		if (DefaultMappingContext)
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}

	URSInputComponent* RSInputComponent = CastChecked<URSInputComponent>(PlayerInputComponent);

	if (!InputConfig)
	{
		UE_LOG(LogTemp, Error, TEXT("%s has no InputConfig"), *GetNameSafe(this));
		return;
	}

	// 한 번의 클릭으로 목적지를 확정하므로 입력을 누른 최초 시점에만 처리합니다
	RSInputComponent->BindNativeAction(InputConfig, RSGameplayTags::InputTag_MoveTo, ETriggerEvent::Started, this, &ThisClass::Input_MoveTo);

	// Ability Input은 입력 태그를 함께 전달하여 ASC의 입력 상태로 기록합니다
	RSInputComponent->BindAbilityActions(InputConfig, this, &ThisClass::Input_AbilityTagPressed, &ThisClass::Input_AbilityTagReleased);
}

void ARSPlayerCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	InitializeAbilitySystem();
}

void ARSPlayerCharacter::Input_MoveTo(const FInputActionValue& InputActionValue)
{
	if (!InputActionValue.Get<bool>() || IsDead())
	{
		return;
	}

	ARSPlayerController* PlayerController = Cast<ARSPlayerController>(Controller);
	if (!PlayerController)
	{
		return;
	}

	FVector MoveToLocation;
	// Controller는 화면 좌표를 월드 위치로 변환하고 Character가 상태 판정과 이동 요청을 소유합니다
	if (!PlayerController->GetMoveToLocation(MoveToLocation))
	{
		return;
	}

	UAIBlueprintHelperLibrary::SimpleMoveToLocation(Controller, MoveToLocation);
}

void ARSPlayerCharacter::Input_AbilityTagPressed(FGameplayTag InputTag)
{
	URSAbilitySystemComponent* AbilitySystemComp = Cast<URSAbilitySystemComponent>(GetAbilitySystemComponent());

	if (!AbilitySystemComp)
	{
		return;
	}

	// 캐릭터는 입력 태그만 전달하고 Spec 검색과 상태 기록은 ASC가 담당합니다
	AbilitySystemComp->AbilityInputTagPressed(InputTag);
}

void ARSPlayerCharacter::Input_AbilityTagReleased(FGameplayTag InputTag)
{
	URSAbilitySystemComponent* AbilitySystemComp = Cast<URSAbilitySystemComponent>(GetAbilitySystemComponent());

	if (!AbilitySystemComp)
	{
		return;
	}

	// 해제 태그를 전달하여 ASC가 Held에서 제거하고 Released로 기록하게 합니다
	AbilitySystemComp->AbilityInputTagReleased(InputTag);
}

UAbilitySystemComponent* ARSPlayerCharacter::GetAbilitySystemComponent() const
{
	const ARSPlayerState* RSPlayerState = GetPlayerState<ARSPlayerState>();

	return RSPlayerState ? RSPlayerState->GetAbilitySystemComponent() : nullptr;
}

bool ARSPlayerCharacter::IsDead() const
{
	return HealthComp && HealthComp->IsDead();
}

void ARSPlayerCharacter::InitializeAbilitySystem()
{
	ARSPlayerState* RSPlayerState = GetPlayerState<ARSPlayerState>();
	if (!RSPlayerState)
	{
		// PlayerState가 교체되거나 제거된 경우 이전 ASC의 델리게이트 연결을 정리합니다
		HealthComp->UninitializeFromAbilitySystem();

		return;
	}

	URSAbilitySystemComponent* AbilitySystemComp = RSPlayerState->GetRSAbilitySystemComponent();
	if (!AbilitySystemComp)
	{
		HealthComp->UninitializeFromAbilitySystem();

		return;
	}

	AbilitySystemComp->InitAbilityActorInfo(RSPlayerState, this);

	// ActorInfo 초기화가 끝난 ASC와 HealthSet을 캐릭터의 HealthComponent에 연결합니다
	HealthComp->InitializeWithAbilitySystem(AbilitySystemComp);

	if (AbilitySystemComp->IsOwnerActorAuthoritative() && !bDefaultAbilitiesGranted && DefaultAbilitySet)
	{
		DefaultAbilitySet->GiveToAbilitySystem(AbilitySystemComp, &GrantedAbilityHandles, this);

		bDefaultAbilitiesGranted = true;
	}
}

void ARSPlayerCharacter::HandleDeathStarted(URSHealthComponent* InHealthComponent)
{
	if (InHealthComponent != HealthComp)
	{
		return;
	}

	if (URSAbilitySystemComponent* AbilitySystemComp = Cast<URSAbilitySystemComponent>(GetAbilitySystemComponent()))
	{
		AbilitySystemComp->ClearAbilityInput();
		AbilitySystemComp->CancelAbilities();
	}

	if (Controller)
	{
		// StopMovementImmediately만 호출하면 Navigation 경로 추종이 다음 프레임에 이동을 다시 요청할 수 있습니다
		Controller->StopMovement();
	}

	UCharacterMovementComponent* CharacterMovementComp = GetCharacterMovement();
	CharacterMovementComp->StopMovementImmediately();
	CharacterMovementComp->DisableMovement();

	if (DeathMontage)
	{
		PlayAnimMontage(DeathMontage);
	}

	ReceiveDeathStarted();
}
