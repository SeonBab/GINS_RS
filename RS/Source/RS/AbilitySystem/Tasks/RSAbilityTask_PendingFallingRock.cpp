#include "RSAbilityTask_PendingFallingRock.h"

#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"

URSAbilityTask_PendingFallingRock* URSAbilityTask_PendingFallingRock::CreatePendingFallingRock(UGameplayAbility* OwningAbility, const FTransform& ImpactTransform, const FRSCombatShape& AttackShape, const FRSTelegraphPresentation& TelegraphPresentation, float FallEffectStartDelay, float ImpactDelay, UNiagaraSystem* FallingRockNiagara)
{
	URSAbilityTask_PendingFallingRock* Task = NewAbilityTask<URSAbilityTask_PendingFallingRock>(OwningAbility);
	const FGameplayAbilityActorInfo* ActorInfo = OwningAbility ? OwningAbility->GetCurrentActorInfo() : nullptr;
	AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	Task->TelegraphComp = AvatarActor ? AvatarActor->FindComponentByClass<URSAttackTelegraphComponent>() : nullptr;
	Task->ImpactTransform = ImpactTransform;
	Task->AttackShape = AttackShape;
	Task->TelegraphPresentation = TelegraphPresentation;
	Task->FallEffectStartDelay = FallEffectStartDelay;
	Task->ImpactDelay = ImpactDelay;
	Task->FallingRockNiagara = FallingRockNiagara;

	return Task;
}

void URSAbilityTask_PendingFallingRock::Activate()
{
	Super::Activate();

	UWorld* World = GetWorld();
	URSAttackTelegraphComponent* TelegraphComponent = TelegraphComp.Get();
	if (!World || !TelegraphComponent || ImpactTransform.ContainsNaN() || !AttackShape.IsDataValid() || FallEffectStartDelay < 0.0f || ImpactDelay <= 0.0f || FallEffectStartDelay > ImpactDelay)
	{
		EndTask();

		return;
	}

	TelegraphHandle = TelegraphComponent->ShowShapeWithHandle(AttackShape, ImpactTransform, TelegraphPresentation);

	if (FallEffectStartDelay <= 0.0f)
	{
		HandleFallEffectStart();
	}
	else
	{
		World->GetTimerManager().SetTimer(FallEffectTimerHandle, this, &ThisClass::HandleFallEffectStart, FallEffectStartDelay, false);
	}

	World->GetTimerManager().SetTimer(ImpactTimerHandle, this, &ThisClass::HandleImpact, ImpactDelay, false);
}

void URSAbilityTask_PendingFallingRock::OnDestroy(bool bInOwnerFinished)
{
	ClearTimers();
	CleanupPresentation();

	Super::OnDestroy(bInOwnerFinished);
}

void URSAbilityTask_PendingFallingRock::HandleFallEffectStart()
{
	if (bImpactCommitted || FallingNiagaraComp || !FallingRockNiagara)
	{
		return;
	}

	FallingNiagaraComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, FallingRockNiagara, ImpactTransform.GetLocation(), ImpactTransform.Rotator(), FVector::OneVector, false, true);
}

void URSAbilityTask_PendingFallingRock::HandleImpact()
{
	if (bImpactCommitted)
	{
		return;
	}

	bImpactCommitted = true;

#if WITH_DEV_AUTOMATION_TESTS
	++CommittedImpactCountForTest;
#endif

	ClearTimers();
	CleanupPresentation();

	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnImpact.Broadcast(ImpactTransform, this);
	}

	if (IsActive())
	{
		EndTask();
	}
}

void URSAbilityTask_PendingFallingRock::ClearTimers()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FallEffectTimerHandle);
		World->GetTimerManager().ClearTimer(ImpactTimerHandle);
	}
}

void URSAbilityTask_PendingFallingRock::CleanupPresentation()
{
	if (URSAttackTelegraphComponent* TelegraphComponent = TelegraphComp.Get())
	{
		TelegraphComponent->HideShape(TelegraphHandle);
	}
	TelegraphHandle = INDEX_NONE;

	if (FallingNiagaraComp)
	{
		FallingNiagaraComp->DeactivateImmediate();
		FallingNiagaraComp->DestroyComponent();
		FallingNiagaraComp = nullptr;
	}
}
