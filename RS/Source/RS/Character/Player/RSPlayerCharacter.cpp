// Fill out your copyright notice in the Description page of Project Settings.


#include "RSPlayerCharacter.h"

#include "AbilitySystemComponent.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/CollisionProfile.h"
#include "InputMappingContext.h"
#include "NiagaraFunctionLibrary.h"
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

	OutlineMeshComp = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("OutlineMeshComponent"));
	OutlineMeshComp->SetupAttachment(GetMesh());
	OutlineMeshComp->SetLeaderPoseComponent(GetMesh());
	OutlineMeshComp->bUseBoundsFromLeaderPoseComponent = true;
	OutlineMeshComp->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	OutlineMeshComp->SetGenerateOverlapEvents(false);

	SpringArmComp = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArmComponent"));
	SpringArmComp->SetupAttachment(GetRootComponent());

	CameraComp = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));
	CameraComp->SetupAttachment(SpringArmComp);

	HealthComp = CreateDefaultSubobject<URSHealthComponent>(TEXT("HealthComponent"));

	// 공격 판정이 진영을 콜리전 채널로 구분하므로 Blueprint 설정 누락을 막기 위해 캡슐 프로파일을 코드에서 고정합니다
	GetCapsuleComponent()->SetCollisionProfileName(TEXT("RSPlayerBody"));

	// 클릭 경로가 꺾일 때 CharacterMovement가 현재 진행 방향을 기준으로 캐릭터를 회전시킵니다
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
}

void ARSPlayerCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// 이전 Blueprint 컴포넌트의 설정이 상속돼도 외곽선 Mesh는 Pose와 Bounds만 공유하고 충돌에는 참여하지 않습니다
	OutlineMeshComp->SetAnimInstanceClass(nullptr);
	OutlineMeshComp->SetLeaderPoseComponent(GetMesh(), true);
	OutlineMeshComp->bUseBoundsFromLeaderPoseComponent = true;
	OutlineMeshComp->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	OutlineMeshComp->SetGenerateOverlapEvents(false);

	HealthComp->OnDeathStarted.AddUniqueDynamic(this, &ThisClass::HandleDeathStarted);
}

void ARSPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	// 맵마다 이동 방향이 달라 고정된 카메라 방위를 그대로 쓰면 일부 맵에서 구도가 어긋나므로 PlayerStart 회전을 카메라 방위로 사용합니다
	// bOrientRotationToMovement가 첫 이동에서 액터 Yaw를 바꿔 덮어쓰므로 스폰 방향을 읽을 수 있는 시점은 여기뿐입니다
	// SpringArmComp는 부모의 Yaw를 상속하지 않으므로 상대 Yaw가 그대로 월드 방위가 되며 Pitch와 길이는 Blueprint 값을 유지합니다
	FRotator SpringArmRotation = SpringArmComp->GetRelativeRotation();
	SpringArmRotation.Yaw = GetActorRotation().Yaw;
	SpringArmComp->SetRelativeRotation(SpringArmRotation);
}

void ARSPlayerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UninitializeMovementBlocking();

	Super::EndPlay(EndPlayReason);
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

	// Started의 최초 이동과 같은 프레임에 발생한 Triggered는 Input_MoveTo에서 건너뜁니다
	RSInputComponent->BindNativeAction(InputConfig, RSGameplayTags::InputTag_MoveTo, ETriggerEvent::Started, this, &ThisClass::Input_MoveToStarted);

	// Triggered를 사용해 최초 입력 프레임 이후 우클릭을 누르는 동안 현재 커서 위치를 계속 추적합니다
	RSInputComponent->BindNativeAction(InputConfig, RSGameplayTags::InputTag_MoveTo, ETriggerEvent::Triggered, this, &ThisClass::Input_MoveTo);

	// Ability Input은 입력 태그를 함께 전달하여 ASC의 입력 상태로 기록합니다
	RSInputComponent->BindAbilityActions(InputConfig, this, &ThisClass::Input_AbilityTagPressed, &ThisClass::Input_AbilityTagReleased);
}

void ARSPlayerCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	InitializeAbilitySystem();
}

void ARSPlayerCharacter::Input_MoveToStarted()
{
	if (!CanRequestMoveTo())
	{
		return;
	}

	FVector MoveToLocation;
	if (!TryGetMoveToLocation(MoveToLocation))
	{
		return;
	}

	UAIBlueprintHelperLibrary::SimpleMoveToLocation(Controller, MoveToLocation);
	SpawnMoveClickEffect(MoveToLocation);

	LastInitialMoveToRequestFrame = GFrameCounter;
	LastMoveToUpdateTime = GetWorld()->GetTimeSeconds();
}

void ARSPlayerCharacter::Input_MoveTo(const FInputActionValue& InputActionValue)
{
	if (!InputActionValue.Get<bool>() || !CanRequestMoveTo())
	{
		return;
	}

	// Started가 처리한 최초 프레임에는 같은 위치의 Trace와 Navigation 경로 탐색을 반복하지 않습니다
	if (LastInitialMoveToRequestFrame == GFrameCounter)
	{
		return;
	}

	const double CurrentTime = GetWorld()->GetTimeSeconds();
	if (CurrentTime - LastMoveToUpdateTime < MoveToUpdateInterval)
	{
		return;
	}

	LastMoveToUpdateTime = CurrentTime;

	FVector MoveToLocation;
	if (!TryGetMoveToLocation(MoveToLocation))
	{
		return;
	}

	UAIBlueprintHelperLibrary::SimpleMoveToLocation(Controller, MoveToLocation);
}

bool ARSPlayerCharacter::TryGetMoveToLocation(FVector& OutMoveToLocation) const
{
	const ARSPlayerController* PlayerController = Cast<ARSPlayerController>(Controller);

	// Controller는 화면 좌표를 월드 위치로 변환하고 Character가 상태 판정과 이동 요청을 소유합니다
	return PlayerController && PlayerController->GetCursorWorldLocation(OutMoveToLocation);
}

void ARSPlayerCharacter::SpawnMoveClickEffect(const FVector& Location) const
{
	if (!MoveClickSystem)
	{
		return;
	}

	UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, MoveClickSystem, Location);
}

bool ARSPlayerCharacter::CanRequestMoveTo() const
{
	const UAbilitySystemComponent* AbilitySystemComp = GetAbilitySystemComponent();

	return AbilitySystemComp
		&& !IsDead()
		&& !AbilitySystemComp->HasMatchingGameplayTag(RSGameplayTags::State_Action_Locked)
		&& !AbilitySystemComp->HasMatchingGameplayTag(RSGameplayTags::State_Movement_Blocked);
}

void ARSPlayerCharacter::StopNavigationMovement()
{
	if (Controller)
	{
		// 속도만 중단하면 Navigation 경로 추종이 다음 프레임에 이동을 다시 요청할 수 있습니다
		Controller->StopMovement();
	}

	GetCharacterMovement()->StopMovementImmediately();
}

void ARSPlayerCharacter::InitializeMovementBlocking(UAbilitySystemComponent* AbilitySystemComp)
{
	if (!AbilitySystemComp)
	{
		UninitializeMovementBlocking();

		return;
	}

	if (MovementStateAbilitySystemComp.Get() != AbilitySystemComp || !ActionLockedTagDelegateHandle.IsValid() || !MovementBlockedTagDelegateHandle.IsValid())
	{
		UninitializeMovementBlocking();

		MovementStateAbilitySystemComp = AbilitySystemComp;
		ActionLockedTagDelegateHandle = AbilitySystemComp->RegisterGameplayTagEvent(RSGameplayTags::State_Action_Locked, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &ThisClass::HandleMovementBlockingTagChanged);
		MovementBlockedTagDelegateHandle = AbilitySystemComp->RegisterGameplayTagEvent(RSGameplayTags::State_Movement_Blocked, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &ThisClass::HandleMovementBlockingTagChanged);
	}

	HandleMovementBlockingTagChanged(RSGameplayTags::State_Action_Locked, AbilitySystemComp->GetTagCount(RSGameplayTags::State_Action_Locked));
	HandleMovementBlockingTagChanged(RSGameplayTags::State_Movement_Blocked, AbilitySystemComp->GetTagCount(RSGameplayTags::State_Movement_Blocked));
}

void ARSPlayerCharacter::UninitializeMovementBlocking()
{
	UAbilitySystemComponent* AbilitySystemComp = MovementStateAbilitySystemComp.Get();
	if (AbilitySystemComp)
	{
		if (ActionLockedTagDelegateHandle.IsValid())
		{
			AbilitySystemComp->RegisterGameplayTagEvent(RSGameplayTags::State_Action_Locked, EGameplayTagEventType::NewOrRemoved).Remove(ActionLockedTagDelegateHandle);
		}

		if (MovementBlockedTagDelegateHandle.IsValid())
		{
			AbilitySystemComp->RegisterGameplayTagEvent(RSGameplayTags::State_Movement_Blocked, EGameplayTagEventType::NewOrRemoved).Remove(MovementBlockedTagDelegateHandle);
		}
	}

	MovementStateAbilitySystemComp.Reset();
	ActionLockedTagDelegateHandle.Reset();
	MovementBlockedTagDelegateHandle.Reset();
}

void ARSPlayerCharacter::HandleMovementBlockingTagChanged(FGameplayTag GameplayTag, int32 NewCount)
{
	const bool bIsMovementBlockingTag = GameplayTag == RSGameplayTags::State_Action_Locked || GameplayTag == RSGameplayTags::State_Movement_Blocked;
	if (bIsMovementBlockingTag && NewCount > 0)
	{
		StopNavigationMovement();
	}
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
		UninitializeMovementBlocking();
		HealthComp->UninitializeFromAbilitySystem();

		return;
	}

	URSAbilitySystemComponent* AbilitySystemComp = RSPlayerState->GetRSAbilitySystemComponent();
	if (!AbilitySystemComp)
	{
		UninitializeMovementBlocking();
		HealthComp->UninitializeFromAbilitySystem();

		return;
	}

	AbilitySystemComp->InitAbilityActorInfo(RSPlayerState, this);

	// ActorInfo 초기화가 끝난 ASC와 HealthSet을 캐릭터의 HealthComponent에 연결합니다
	HealthComp->InitializeWithAbilitySystem(AbilitySystemComp);
	InitializeMovementBlocking(AbilitySystemComp);

	if (AbilitySystemComp->IsOwnerActorAuthoritative() && !bDefaultAbilitiesGranted && DefaultAbilitySet)
	{
		DefaultAbilitySet->GiveToAbilitySystem(AbilitySystemComp, &GrantedAbilityHandles, this);

		bDefaultAbilitiesGranted = true;
	}

	// Pawn 소유는 ASC 초기화보다 먼저 일어나므로, 관찰할 준비가 끝난 지금 다시 알려 사용자 인터페이스가 초기화 순서에 의존하지 않게 합니다
	if (ARSPlayerController* RSPlayerController = GetController<ARSPlayerController>())
	{
		RSPlayerController->RegisterViewModelSource(this);
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

	StopNavigationMovement();
	GetCharacterMovement()->DisableMovement();

	if (DeathMontage)
	{
		PlayAnimMontage(DeathMontage);
	}

	ReceiveDeathStarted();
}
