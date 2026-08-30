// Fill out your copyright notice in the Description page of Project Settings.


#include "RSPlayerCharacter.h"

#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "RSAbilitySystemComponent.h"
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

	// Native Input은 캐릭터가 직접 처리할 콜백에 바인딩합니다
	RSInputComponent->BindNativeAction(InputConfig, RSGameplayTags::InputTag_Move, ETriggerEvent::Triggered, this, &ThisClass::Input_Move);

	// Ability Input은 입력 태그를 함께 전달하여 ASC의 입력 상태로 기록합니다
	RSInputComponent->BindAbilityActions(InputConfig, this, &ThisClass::Input_AbilityTagPressed, &ThisClass::Input_AbilityTagReleased);
}

void ARSPlayerCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	InitializeAbilitySystem();
}

void ARSPlayerCharacter::Input_Move(const FInputActionValue& InputActionValue)
{
	if (!Controller)
	{
		return;
	}

	const FVector2D MovementValue = InputActionValue.Get<FVector2D>();

	// 컨트롤러의 Yaw 회전을 기준으로 이동 방향을 계산합니다
	const FRotator ControlRotation = Controller->GetControlRotation();
	const FRotator YawRotation(0.0f, ControlRotation.Yaw, 0.0f);

	// 카메라 기준 이동이 필요하면 아래 회전을 대신 사용합니다
	// const FRotator CameraRotation = CameraComp->GetComponentRotation();
	// const FRotator YawRotation(0.0f, CameraRotation.Yaw, 0.0f);

	const FVector ForwardDirection = YawRotation.RotateVector(FVector::ForwardVector);
	const FVector RightDirection = YawRotation.RotateVector(FVector::RightVector);

	AddMovementInput(ForwardDirection, MovementValue.Y);
	AddMovementInput(RightDirection, MovementValue.X);
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

void ARSPlayerCharacter::InitializeAbilitySystem()
{
	ARSPlayerState* RSPlayerState = GetPlayerState<ARSPlayerState>();
	if (!RSPlayerState)
	{
		return;
	}

	URSAbilitySystemComponent* AbilitySystemComp = RSPlayerState->GetRSAbilitySystemComponent();
	if (!AbilitySystemComp)
	{
		return;
	}

	AbilitySystemComp->InitAbilityActorInfo(RSPlayerState, this);

	if (AbilitySystemComp->IsOwnerActorAuthoritative() && !bDefaultAbilitiesGranted && DefaultAbilitySet)
	{
		DefaultAbilitySet->GiveToAbilitySystem(AbilitySystemComp, &GrantedAbilityHandles, this);

		bDefaultAbilitiesGranted = true;
	}
}
