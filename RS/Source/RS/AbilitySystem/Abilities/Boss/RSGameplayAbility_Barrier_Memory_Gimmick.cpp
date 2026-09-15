// Fill out your copyright notice in the Description page of Project Settings.

#include "RSGameplayAbility_Barrier_Memory_Gimmick.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "DrawDebugHelpers.h"
#include "GameplayEffect.h"
#include "NiagaraComponent.h"
#include "RSGameplayTags.h"
#include "RSHealthSet.h"
#include "RSPlayerCharacter.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#include "RSAnimNotify_GameplayEvent.h"
#endif

namespace
{
	constexpr int32 PatternAttackCount = 3;
	constexpr int32 BarrierZoneCount = 4;
	constexpr float FirstBarrierAngleDegrees = 45.0f;
	constexpr float BarrierAngleStepDegrees = 90.0f;

	/** 방어막을 만들 때 그린 원은 표시 크기와 판정 반지름을 비교할 수 있도록 예고와 세 공격 동안 남깁니다 */
	constexpr float BarrierFieldDebugLifeTime = 30.0f;

	/** 판정 프레임의 안전지대는 다음 공격의 원과 섞이지 않게 짧게만 남깁니다 */
	constexpr float BarrierHitCheckDebugLifeTime = 1.0f;
}

bool FRSBarrierFieldDefinition::IsDataValid(FString* OutValidationError) const
{
	auto SetError = [OutValidationError](const FString& Message)
	{
		if (OutValidationError)
		{
			*OutValidationError = Message;
		}
	};

	if (!FMath::IsFinite(DistanceFromCenter) || DistanceFromCenter <= 0.0f || !FMath::IsFinite(SafeRadius) || SafeRadius <= 0.0f)
	{
		SetError(TEXT("DistanceFromCenter and SafeRadius must be finite and positive."));

		return false;
	}

	if (SafeRadius >= DistanceFromCenter / FMath::Sqrt(2.0f))
	{
		SetError(TEXT("SafeRadius must be smaller than DistanceFromCenter / sqrt(2)."));

		return false;
	}

	FString NiagaraError;
	if (!RedNiagara.IsDataValid(&NiagaraError))
	{
		SetError(FString::Printf(TEXT("RedNiagara is invalid: %s"), *NiagaraError));

		return false;
	}

	if (!YellowNiagara.IsDataValid(&NiagaraError))
	{
		SetError(FString::Printf(TEXT("YellowNiagara is invalid: %s"), *NiagaraError));

		return false;
	}

	return true;
}

URSGameplayAbility_Barrier_Memory_Gimmick::URSGameplayAbility_Barrier_Memory_Gimmick()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	ActivationBlockedTags.AddTag(RSGameplayTags::State_Action_Locked);

	AttackShape.Type = ERSCombatShapeType::Sphere;
	AttackShape.InnerRadius = 0.0f;
	AttackShape.Radius = 0.0f;
}

ERSBarrierColor URSGameplayAbility_Barrier_Memory_Gimmick::GetAlternatingColor(ERSBarrierColor StartingColorValue, int32 Index)
{
	return Index % 2 == 0 ? StartingColorValue : (StartingColorValue == ERSBarrierColor::Red ? ERSBarrierColor::Yellow : ERSBarrierColor::Red);
}

bool URSGameplayAbility_Barrier_Memory_Gimmick::IsLocationInsideSafeZone(const FVector& Location, const FVector& ZoneCenter, float SafeRadius)
{
	if (!FMath::IsFinite(SafeRadius) || SafeRadius < 0.0f)
	{
		return false;
	}

	return FVector::DistSquared2D(Location, ZoneCenter) <= FMath::Square(SafeRadius);
}

FTransform URSGameplayAbility_Barrier_Memory_Gimmick::CalculateBeamTransform(const FTransform& PatternTransform, const FVector& LocalOffset)
{
	return FTransform(PatternTransform.GetRotation(), PatternTransform.TransformPosition(LocalOffset));
}

void URSGameplayAbility_Barrier_Memory_Gimmick::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	bIsEndingAbility = false;
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	EndGameplayEventTasks();
	DestroyBarrierZones();
	PatternCenterTransform = FTransform::Identity;
	PreviewEventCount = 0;
	AttackIndex = 0;
	bReceivedAttackPresentation = false;
	bReceivedHitCheck = false;

	const AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	FString RuntimeValidationError;
	const bool bHasValidAttackShape = AttackShape.Type == ERSCombatShapeType::Sphere && AttackShape.InnerRadius == 0.0f && AttackShape.IsDataValid(&RuntimeValidationError)
		&& AttackShape.Radius >= BarrierField.DistanceFromCenter + BarrierField.SafeRadius;
	if (!AvatarActor || !ActorInfo->AbilitySystemComponent.IsValid() || !RoarMontage || !AttackMontage
		|| !FMath::IsFinite(RoarMontagePlayRate) || RoarMontagePlayRate <= 0.0f || !FMath::IsFinite(AttackMontagePlayRate) || AttackMontagePlayRate <= 0.0f
		|| !BarrierField.IsDataValid(&RuntimeValidationError) || !bHasValidAttackShape)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	// 방어막과 공격 연출이 보스 발밑 평면에 놓이도록 액터 원점이 아니라 캡슐 바닥을 패턴 원점으로 씁니다
	FVector PatternCenterLocation = FVector::ZeroVector;
	if (!URSCombatFunctionLibrary::TryGetActorGroundLocation(AvatarActor, PatternCenterLocation))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	// 세 번의 공격이 같은 곳에 Beam을 내도록 전방을 시작 순간에 한 번만 고정하며, 방어막 각도는 이 회전을 쓰지 않아 월드 기준을 유지합니다
	FVector HorizontalForward = AvatarActor->GetActorForwardVector();
	HorizontalForward.Z = 0.0f;
	if (!HorizontalForward.Normalize())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	PatternCenterTransform = FTransform(HorizontalForward.Rotation(), PatternCenterLocation);
	StartingColor = FMath::RandBool() ? ERSBarrierColor::Red : ERSBarrierColor::Yellow;

	CreateBarrierField();
	if (!CreateBarrierPresentations())
	{
		CancelForInvalidRuntime(TEXT("could not create all barrier presentations"));

		return;
	}

	DrawDebugBarrierZones(BarrierFieldDebugLifeTime, false);
	StartPreviewMontage();
}

void URSGameplayAbility_Barrier_Memory_Gimmick::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (bIsEndingAbility)
	{
		return;
	}

	bIsEndingAbility = true;
	EndGameplayEventTasks();
	DestroyBarrierZones();
	bReceivedAttackPresentation = false;
	bReceivedHitCheck = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	bIsEndingAbility = false;
}

void URSGameplayAbility_Barrier_Memory_Gimmick::CreateBarrierField()
{
	BarrierZones.Reset();
	BarrierZones.Reserve(BarrierZoneCount);

	for (int32 ZoneIndex = 0; ZoneIndex < BarrierZoneCount; ++ZoneIndex)
	{
		const float AngleRadians = FMath::DegreesToRadians(FirstBarrierAngleDegrees + BarrierAngleStepDegrees * ZoneIndex);
		FRSBarrierZoneRuntimeState& Zone = BarrierZones.AddDefaulted_GetRef();
		Zone.Center = PatternCenterTransform.GetLocation() + FVector(FMath::Cos(AngleRadians), FMath::Sin(AngleRadians), 0.0f) * BarrierField.DistanceFromCenter;
		Zone.Color = ZoneIndex % 2 == 0 ? ERSBarrierColor::Red : ERSBarrierColor::Yellow;
	}
}

bool URSGameplayAbility_Barrier_Memory_Gimmick::CreateBarrierPresentations()
{
	bool bCreatedAllPresentations = true;
	for (FRSBarrierZoneRuntimeState& Zone : BarrierZones)
	{
		Zone.Presentation = CreateBarrierPresentation(Zone);
		bCreatedAllPresentations &= Zone.Presentation.IsValid();
	}

	return bCreatedAllPresentations;
}

UNiagaraComponent* URSGameplayAbility_Barrier_Memory_Gimmick::CreateBarrierPresentation(const FRSBarrierZoneRuntimeState& Zone)
{
	const FRSNiagaraSpawnDefinition& Definition = Zone.Color == ERSBarrierColor::Red ? BarrierField.RedNiagara : BarrierField.YellowNiagara;

	return SpawnNiagaraFromDefinition(Definition, FTransform(Zone.Center), false);
}

void URSGameplayAbility_Barrier_Memory_Gimmick::DestroyBarrierZones()
{
	for (FRSBarrierZoneRuntimeState& Zone : BarrierZones)
	{
		if (UNiagaraComponent* Presentation = Zone.Presentation.Get())
		{
			Presentation->DestroyComponent();
		}
	}

	BarrierZones.Reset();
}

void URSGameplayAbility_Barrier_Memory_Gimmick::DrawDebugBarrierZones(float LifeTime, bool bHighlightCurrentColor) const
{
#if ENABLE_DRAW_DEBUG
	if (!URSCombatFunctionLibrary::IsHitCheckDebugEnabled())
	{
		return;
	}

	const ERSBarrierColor CurrentColor = GetAlternatingColor(StartingColor, AttackIndex);

	FRSCombatShape ZoneShape;
	ZoneShape.Type = ERSCombatShapeType::Sphere;
	ZoneShape.Radius = BarrierField.SafeRadius;

	for (const FRSBarrierZoneRuntimeState& Zone : BarrierZones)
	{
		// 이번 공격에 안전한 영역만 자기 색으로 남기면 어디로 가야 했는지가 판정 프레임 기준으로 그대로 보입니다
		const bool bIsSafeNow = !bHighlightCurrentColor || Zone.Color == CurrentColor;
		const FColor UnsafeZoneColor(80, 80, 80);
		const FColor ZoneColor = bIsSafeNow ? (Zone.Color == ERSBarrierColor::Red ? FColor::Red : FColor::Yellow) : UnsafeZoneColor;

		URSCombatFunctionLibrary::DrawDebugCombatShape(GetWorld(), ZoneShape, FTransform(Zone.Center), ZoneColor, LifeTime);
	}
#endif
}

bool URSGameplayAbility_Barrier_Memory_Gimmick::IsPlayerInsideCurrentSafeZone(const AActor& PlayerActor) const
{
	const ERSBarrierColor CurrentColor = GetAlternatingColor(StartingColor, AttackIndex);

	for (const FRSBarrierZoneRuntimeState& Zone : BarrierZones)
	{
		if (Zone.Color == CurrentColor && IsLocationInsideSafeZone(PlayerActor.GetActorLocation(), Zone.Center, BarrierField.SafeRadius))
		{
			return true;
		}
	}

	return false;
}

void URSGameplayAbility_Barrier_Memory_Gimmick::StartPreviewMontage()
{
	PreviewEventCount = 0;

	PresentationEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, RSGameplayTags::GameplayEvent_Presentation_Niagara, nullptr, false);
	PresentationEventTask->EventReceived.AddDynamic(this, &ThisClass::HandleEyeNiagaraEvent);
	PresentationEventTask->ReadyForActivation();

	UAbilityTask_PlayMontageAndWait* MontageTask = PlayPatternMontage(RoarMontage, RoarMontagePlayRate);
	if (!MontageTask)
	{
		CancelForInvalidRuntime(TEXT("could not play RoarMontage"));

		return;
	}

	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandlePreviewMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleMontageCancelled);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleMontageCancelled);

	const UAnimInstance* AnimInstance = CurrentActorInfo ? CurrentActorInfo->GetAnimInstance() : nullptr;
	if (!AnimInstance || !AnimInstance->Montage_IsPlaying(RoarMontage))
	{
		CancelForInvalidRuntime(TEXT("could not start RoarMontage"));
	}
}

void URSGameplayAbility_Barrier_Memory_Gimmick::StartAttackMontage()
{
	EndGameplayEventTasks();
	bReceivedAttackPresentation = false;
	bReceivedHitCheck = false;

	PresentationEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, RSGameplayTags::GameplayEvent_Presentation_Niagara, nullptr, false);
	PresentationEventTask->EventReceived.AddDynamic(this, &ThisClass::HandleSwordNiagaraEvent);
	PresentationEventTask->ReadyForActivation();

	HitCheckEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, RSGameplayTags::GameplayEvent_Combat_HitCheck, nullptr, false);
	HitCheckEventTask->EventReceived.AddDynamic(this, &ThisClass::HandleHitCheckEvent);
	HitCheckEventTask->ReadyForActivation();

	UAbilityTask_PlayMontageAndWait* MontageTask = PlayPatternMontage(AttackMontage, AttackMontagePlayRate);
	if (!MontageTask)
	{
		CancelForInvalidRuntime(TEXT("could not play AttackMontage"));

		return;
	}

	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleAttackMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleMontageCancelled);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleMontageCancelled);

	const UAnimInstance* AnimInstance = CurrentActorInfo ? CurrentActorInfo->GetAnimInstance() : nullptr;
	if (!AnimInstance || !AnimInstance->Montage_IsPlaying(AttackMontage))
	{
		CancelForInvalidRuntime(TEXT("could not start AttackMontage"));
	}
}

void URSGameplayAbility_Barrier_Memory_Gimmick::EndGameplayEventTasks()
{
	if (PresentationEventTask)
	{
		PresentationEventTask->EndTask();
		PresentationEventTask = nullptr;
	}

	if (HitCheckEventTask)
	{
		HitCheckEventTask->EndTask();
		HitCheckEventTask = nullptr;
	}
}

void URSGameplayAbility_Barrier_Memory_Gimmick::HandleEyeNiagaraEvent(FGameplayEventData Payload)
{
	if (PreviewEventCount >= PatternAttackCount)
	{
		CancelForInvalidRuntime(TEXT("received too many preview presentation events"));

		return;
	}

	const ERSBarrierColor Color = GetAlternatingColor(StartingColor, PreviewEventCount);
	const FRSNiagaraSpawnDefinition& Definition = Color == ERSBarrierColor::Red ? RedEyeNiagara : YellowEyeNiagara;
	if (!SpawnNiagaraFromDefinition(Definition, PatternCenterTransform, true))
	{
		CancelForInvalidRuntime(TEXT("could not create an eye presentation"));

		return;
	}

	++PreviewEventCount;
}

void URSGameplayAbility_Barrier_Memory_Gimmick::HandleSwordNiagaraEvent(FGameplayEventData Payload)
{
	if (bReceivedAttackPresentation || bReceivedHitCheck)
	{
		CancelForInvalidRuntime(TEXT("received an extra or out-of-order attack presentation event"));

		return;
	}

	const ERSBarrierColor Color = GetAlternatingColor(StartingColor, AttackIndex);
	const FRSNiagaraSpawnDefinition& Definition = Color == ERSBarrierColor::Red ? RedSwordNiagara : YellowSwordNiagara;
	if (!SpawnNiagaraFromDefinition(Definition, PatternCenterTransform, true))
	{
		CancelForInvalidRuntime(TEXT("could not create a sword presentation"));

		return;
	}

	bReceivedAttackPresentation = true;
}

void URSGameplayAbility_Barrier_Memory_Gimmick::HandleHitCheckEvent(FGameplayEventData Payload)
{
	if (!bReceivedAttackPresentation || bReceivedHitCheck)
	{
		CancelForInvalidRuntime(TEXT("received an extra or out-of-order hit check event"));

		return;
	}

	bReceivedHitCheck = true;
	DrawDebugBarrierZones(BarrierHitCheckDebugLifeTime, true);
	PlayBeamPresentation();
	ExecuteCurrentHitCheck();
}

void URSGameplayAbility_Barrier_Memory_Gimmick::PlayBeamPresentation()
{
	const ERSBarrierColor Color = GetAlternatingColor(StartingColor, AttackIndex);
	const FRSNiagaraSpawnDefinition& Definition = Color == ERSBarrierColor::Red ? RedBeamNiagara : YellowBeamNiagara;

	// 이미 보이는 공격에 얹는 연출이고 이 시점에는 피해가 나가는 중이므로, 생성하지 못해도 패턴을 취소하지 않고 기록만 남깁니다
	if (!SpawnNiagaraFromDefinition(Definition, CalculateBeamTransform(PatternCenterTransform, BeamOffset), true))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s could not create a beam presentation for attack %d"), *GetName(), AttackIndex);
	}
}

void URSGameplayAbility_Barrier_Memory_Gimmick::HandlePreviewMontageCompleted()
{
	if (!IsActive())
	{
		return;
	}

	if (PreviewEventCount != PatternAttackCount)
	{
		CancelForInvalidRuntime(TEXT("RoarMontage did not emit exactly three presentation events"));

		return;
	}

	StartAttackMontage();
}

void URSGameplayAbility_Barrier_Memory_Gimmick::HandleAttackMontageCompleted()
{
	if (!IsActive())
	{
		return;
	}

	if (!bReceivedAttackPresentation || !bReceivedHitCheck)
	{
		CancelForInvalidRuntime(TEXT("AttackMontage did not emit one presentation event followed by one hit check"));

		return;
	}

	++AttackIndex;
	if (AttackIndex >= PatternAttackCount)
	{
		EndGameplayEventTasks();
		DestroyBarrierZones();
		FinishPatternWhenMontageEnds();

		return;
	}

	StartAttackMontage();
}

void URSGameplayAbility_Barrier_Memory_Gimmick::HandleMontageCancelled()
{
	if (!bIsEndingAbility && IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

void URSGameplayAbility_Barrier_Memory_Gimmick::ExecuteCurrentHitCheck()
{
	AActor* AvatarActor = CurrentActorInfo ? CurrentActorInfo->AvatarActor.Get() : nullptr;
	if (!AvatarActor)
	{
		CancelForInvalidRuntime(TEXT("lost its avatar before hit check"));

		return;
	}

	TArray<AActor*> HitTargets;
	URSCombatFunctionLibrary::FindTargetsInShape(AvatarActor, TargetChannel, AttackShape, PatternCenterTransform, HitTargets);

	const float DamageAmount = Damage.GetValueAtLevel(GetAbilityLevel(CurrentSpecHandle, CurrentActorInfo));
	for (AActor* HitTarget : HitTargets)
	{
		ARSPlayerCharacter* PlayerCharacter = Cast<ARSPlayerCharacter>(HitTarget);
		if (PlayerCharacter && !IsPlayerInsideCurrentSafeZone(*PlayerCharacter))
		{
			ApplyDamageToTarget(PlayerCharacter, DamageEffectClass, DamageAmount);
		}
	}
}

void URSGameplayAbility_Barrier_Memory_Gimmick::CancelForInvalidRuntime(const TCHAR* Reason)
{
	UE_LOG(LogTemp, Warning, TEXT("%s cancelled because it %s"), *GetName(), Reason);

	if (!bIsEndingAbility && IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

#if WITH_EDITOR
EDataValidationResult URSGameplayAbility_Barrier_Memory_Gimmick::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult ValidationResult = Super::IsDataValid(Context);

	auto AddError = [&Context, &ValidationResult](const FString& Message)
	{
		Context.AddError(FText::FromString(Message));
		ValidationResult = EDataValidationResult::Invalid;
	};

	if (!FMath::IsFinite(RoarMontagePlayRate) || RoarMontagePlayRate <= 0.0f || !FMath::IsFinite(AttackMontagePlayRate) || AttackMontagePlayRate <= 0.0f)
	{
		AddError(TEXT("RoarMontagePlayRate and AttackMontagePlayRate must be finite and positive."));
	}

	FString BarrierError;
	if (!BarrierField.IsDataValid(&BarrierError))
	{
		AddError(FString::Printf(TEXT("BarrierField is invalid: %s"), *BarrierError));
	}

	FString ShapeError;
	if (AttackShape.Type != ERSCombatShapeType::Sphere || !AttackShape.IsDataValid(&ShapeError) || AttackShape.InnerRadius != 0.0f)
	{
		AddError(FString::Printf(TEXT("AttackShape must be a valid Sphere with InnerRadius 0: %s"), *ShapeError));
	}
	else if (AttackShape.Radius < BarrierField.DistanceFromCenter + BarrierField.SafeRadius)
	{
		AddError(TEXT("AttackShape.Radius must cover DistanceFromCenter + SafeRadius."));
	}

	const TPair<const TCHAR*, const FRSNiagaraSpawnDefinition*> NiagaraDefinitions[] =
	{
		{ TEXT("RedEyeNiagara"), &RedEyeNiagara },
		{ TEXT("YellowEyeNiagara"), &YellowEyeNiagara },
		{ TEXT("RedSwordNiagara"), &RedSwordNiagara },
		{ TEXT("YellowSwordNiagara"), &YellowSwordNiagara },
		{ TEXT("RedBeamNiagara"), &RedBeamNiagara },
		{ TEXT("YellowBeamNiagara"), &YellowBeamNiagara }
	};

	for (const TPair<const TCHAR*, const FRSNiagaraSpawnDefinition*>& Pair : NiagaraDefinitions)
	{
		FString DefinitionError;
		if (!Pair.Value->IsDataValid(&DefinitionError))
		{
			AddError(FString::Printf(TEXT("%s is invalid: %s"), Pair.Key, *DefinitionError));
		}
	}

	if (RedEyeNiagara.SpawnMode == ERSNiagaraSpawnMode::WorldTransform || YellowEyeNiagara.SpawnMode == ERSNiagaraSpawnMode::WorldTransform
		|| RedSwordNiagara.SpawnMode == ERSNiagaraSpawnMode::WorldTransform || YellowSwordNiagara.SpawnMode == ERSNiagaraSpawnMode::WorldTransform)
	{
		AddError(TEXT("Eye and sword Niagara definitions must use a socket spawn mode."));
	}

	if (BarrierField.RedNiagara.SpawnMode != ERSNiagaraSpawnMode::WorldTransform || BarrierField.YellowNiagara.SpawnMode != ERSNiagaraSpawnMode::WorldTransform)
	{
		AddError(TEXT("Barrier Niagara definitions must use WorldTransform."));
	}

	// Beam은 소켓이 아니라 패턴이 고정한 전방 위치에 놓이므로 소켓 기준 생성은 BeamOffset을 무시하게 됩니다
	if (RedBeamNiagara.SpawnMode != ERSNiagaraSpawnMode::WorldTransform || YellowBeamNiagara.SpawnMode != ERSNiagaraSpawnMode::WorldTransform)
	{
		AddError(TEXT("Beam Niagara definitions must use WorldTransform."));
	}

	if (BeamOffset.ContainsNaN())
	{
		AddError(TEXT("BeamOffset must be finite."));
	}

	auto CountNotify = [](const UAnimMontage* Montage, const FGameplayTag& EventTag)
	{
		int32 Count = 0;
		if (Montage)
		{
			for (const FAnimNotifyEvent& NotifyEvent : Montage->Notifies)
			{
				const URSAnimNotify_GameplayEvent* GameplayEventNotify = Cast<URSAnimNotify_GameplayEvent>(NotifyEvent.Notify);
				if (GameplayEventNotify && GameplayEventNotify->GetEventTag().MatchesTagExact(EventTag))
				{
					++Count;
				}
			}
		}

		return Count;
	};

	if (!RoarMontage || CountNotify(RoarMontage, RSGameplayTags::GameplayEvent_Presentation_Niagara) != PatternAttackCount)
	{
		AddError(TEXT("RoarMontage must contain exactly three GameplayEvent.Presentation.Niagara notifies."));
	}

	if (!AttackMontage || CountNotify(AttackMontage, RSGameplayTags::GameplayEvent_Presentation_Niagara) != 1 || CountNotify(AttackMontage, RSGameplayTags::GameplayEvent_Combat_HitCheck) != 1)
	{
		AddError(TEXT("AttackMontage must contain exactly one presentation notify and one hit check notify."));
	}
	else
	{
		float PresentationTime = 0.0f;
		float HitCheckTime = 0.0f;
		for (const FAnimNotifyEvent& NotifyEvent : AttackMontage->Notifies)
		{
			const URSAnimNotify_GameplayEvent* GameplayEventNotify = Cast<URSAnimNotify_GameplayEvent>(NotifyEvent.Notify);
			if (!GameplayEventNotify)
			{
				continue;
			}

			if (GameplayEventNotify->GetEventTag().MatchesTagExact(RSGameplayTags::GameplayEvent_Presentation_Niagara))
			{
				PresentationTime = NotifyEvent.GetTriggerTime();
			}
			else if (GameplayEventNotify->GetEventTag().MatchesTagExact(RSGameplayTags::GameplayEvent_Combat_HitCheck))
			{
				HitCheckTime = NotifyEvent.GetTriggerTime();
			}
		}

		if (PresentationTime >= HitCheckTime)
		{
			AddError(TEXT("AttackMontage presentation notify must occur before its hit check notify."));
		}
	}

	if (!URSCombatFunctionLibrary::ValidateIntegerDamage(Damage, TEXT("Damage"), Context))
	{
		ValidationResult = EDataValidationResult::Invalid;
	}

	const UGameplayEffect* DamageEffect = DamageEffectClass ? DamageEffectClass->GetDefaultObject<UGameplayEffect>() : nullptr;
	if (!DamageEffect)
	{
		AddError(TEXT("DamageEffectClass is required."));
	}
	else
	{
		float Duration = 0.0f;
		const bool bHasStaticDuration = DamageEffect->DurationMagnitude.GetStaticMagnitudeIfPossible(1.0f, Duration);
		const float Period = DamageEffect->Period.GetValueAtLevel(1.0f);
		if (DamageEffect->DurationPolicy != EGameplayEffectDurationType::HasDuration || !bHasStaticDuration || !FMath::IsFinite(Duration) || Duration <= 0.0f
			|| !FMath::IsFinite(Period) || Period <= 0.0f || !DamageEffect->bExecutePeriodicEffectOnApplication || DamageEffect->GetStackingType() != EGameplayEffectStackingType::None)
		{
			AddError(TEXT("DamageEffectClass must be a finite positive-duration periodic effect that executes on application and does not stack."));
		}

		bool bHasValidDamageModifier = false;
		for (const FGameplayModifierInfo& Modifier : DamageEffect->Modifiers)
		{
			if (Modifier.Attribute == URSHealthSet::GetDamageAttribute()
				&& Modifier.ModifierOp == EGameplayModOp::Additive
				&& Modifier.ModifierMagnitude.GetMagnitudeCalculationType() == EGameplayEffectMagnitudeCalculation::SetByCaller
				&& Modifier.ModifierMagnitude.GetSetByCallerFloat().DataTag.MatchesTagExact(RSGameplayTags::SetByCaller_Damage))
			{
				bHasValidDamageModifier = true;
				break;
			}
		}

		if (!bHasValidDamageModifier)
		{
			AddError(TEXT("DamageEffectClass needs an Additive URSHealthSet::Damage modifier using SetByCaller.Damage."));
		}
	}

	return ValidationResult;
}
#endif
