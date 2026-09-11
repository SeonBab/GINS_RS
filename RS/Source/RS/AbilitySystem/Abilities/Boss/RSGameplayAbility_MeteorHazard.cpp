#include "RSGameplayAbility_MeteorHazard.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Actors/RSBossMeteorHazard.h"
#include "RSBossController.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "RSGameplayTags.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

URSGameplayAbility_MeteorHazard::URSGameplayAbility_MeteorHazard()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(RSGameplayTags::Ability_Combat_MeteorHazard);
	SetAssetTags(AssetTags);

	ActivationBlockedTags.AddTag(RSGameplayTags::State_Action_Locked);
}

void URSGameplayAbility_MeteorHazard::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	CapturedTargetActor.Reset();
	ActiveMeteorHazard = nullptr;
	PreparationFinishedDelegateHandle.Reset();
	bHazardActivated = false;
	bRoarMontageFinished = !RoarMontage;

	if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid() || !MeteorHazardClass || !CaptureTargetActor(ActorInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	// 포효는 표현만 담당하므로 재생 실패나 중단이 운석의 게임플레이 시간을 취소하지 않습니다
	if (RoarMontage)
	{
		UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, RoarMontage);
		MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleRoarMontageFinished);
		MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::HandleRoarMontageFinished);
		MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleRoarMontageFinished);
		MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleRoarMontageFinished);
		MontageTask->ReadyForActivation();
	}

	if (AttackStartDelay <= 0.0f)
	{
		HandleStartDelayFinished();

		return;
	}

	UAbilityTask_WaitDelay* StartDelayTask = UAbilityTask_WaitDelay::WaitDelay(this, AttackStartDelay);
	StartDelayTask->OnFinish.AddDynamic(this, &ThisClass::HandleStartDelayFinished);
	StartDelayTask->ReadyForActivation();
}

void URSGameplayAbility_MeteorHazard::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	ARSBossMeteorHazard* MeteorHazard = ActiveMeteorHazard;
	if (MeteorHazard && PreparationFinishedDelegateHandle.IsValid())
	{
		MeteorHazard->OnPreparationFinished().Remove(PreparationFinishedDelegateHandle);
	}

	PreparationFinishedDelegateHandle.Reset();
	ActiveMeteorHazard = nullptr;
	CapturedTargetActor.Reset();
	bHazardActivated = false;
	bRoarMontageFinished = false;

	if (bWasCancelled && MeteorHazard)
	{
		MeteorHazard->RequestCleanup();
	}

	EndAnimationGameplayStatesForMontage(ActorInfo, RoarMontage);

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

#if WITH_EDITOR
EDataValidationResult URSGameplayAbility_MeteorHazard::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult ValidationResult = Super::IsDataValid(Context);

	if (!MeteorHazardClass)
	{
		Context.AddError(FText::FromString(TEXT("MeteorHazardClass is not configured.")));
		ValidationResult = EDataValidationResult::Invalid;
	}

	if (!FMath::IsFinite(AttackStartDelay) || AttackStartDelay < 0.0f)
	{
		Context.AddError(FText::FromString(TEXT("AttackStartDelay must be a finite non-negative value.")));
		ValidationResult = EDataValidationResult::Invalid;
	}

	return ValidationResult;
}
#endif

bool URSGameplayAbility_MeteorHazard::CaptureTargetActor(const FGameplayAbilityActorInfo* ActorInfo)
{
	const APawn* BossPawn = ActorInfo ? Cast<APawn>(ActorInfo->AvatarActor.Get()) : nullptr;
	const ARSBossController* BossController = BossPawn ? Cast<ARSBossController>(BossPawn->GetController()) : nullptr;
	AActor* TargetActor = BossController ? BossController->GetTargetActor() : nullptr;
	if (!BossController || !BossController->IsTargetActorValid(TargetActor))
	{
		return false;
	}

	CapturedTargetActor = TargetActor;

	return true;
}

void URSGameplayAbility_MeteorHazard::HandleStartDelayFinished()
{
	if (!IsActive() || ActiveMeteorHazard)
	{
		return;
	}

	UWorld* World = GetWorld();
	AActor* AvatarActor = CurrentActorInfo ? CurrentActorInfo->AvatarActor.Get() : nullptr;
	AActor* TargetActor = CapturedTargetActor.Get();
	if (!World || !AvatarActor || !IsValid(TargetActor))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	const FTransform SpawnTransform(TargetActor->GetActorLocation());
	ARSBossMeteorHazard* MeteorHazard = World->SpawnActorDeferred<ARSBossMeteorHazard>(MeteorHazardClass, SpawnTransform, AvatarActor, Cast<APawn>(AvatarActor), ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!MeteorHazard)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	MeteorHazard->Initialize(TargetActor);
	PreparationFinishedDelegateHandle = MeteorHazard->OnPreparationFinished().AddUObject(this, &ThisClass::HandlePreparationFinished);
	ActiveMeteorHazard = MeteorHazard;
	UGameplayStatics::FinishSpawningActor(MeteorHazard, SpawnTransform);
}

void URSGameplayAbility_MeteorHazard::HandlePreparationFinished(ERSBossMeteorHazardPreparationResult PreparationResult)
{
	if (!IsActive())
	{
		return;
	}

	if (PreparationResult == ERSBossMeteorHazardPreparationResult::CleanedUp)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	bHazardActivated = true;
	TryFinishAbility();
}

void URSGameplayAbility_MeteorHazard::HandleRoarMontageFinished()
{
	if (!IsActive())
	{
		return;
	}

	bRoarMontageFinished = true;
	TryFinishAbility();
}

void URSGameplayAbility_MeteorHazard::TryFinishAbility()
{
	if (IsActive() && bHazardActivated && bRoarMontageFinished)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}
