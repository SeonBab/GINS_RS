// Fill out your copyright notice in the Description page of Project Settings.

#include "RSGameplayAbility_Barrier_Collapse_Gimmick.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "DrawDebugHelpers.h"
#include "GameplayEffect.h"
#include "NiagaraComponent.h"
#include "Tasks/RSAbilityTask_BossFacing.h"
#include "RSBossCharacter.h"
#include "RSBossController.h"
#include "RSGameplayAbility_Barrier_Memory_Gimmick.h"
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

URSGameplayAbility_Barrier_Collapse_Gimmick::URSGameplayAbility_Barrier_Collapse_Gimmick()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	ActivationBlockedTags.AddTag(RSGameplayTags::State_Action_Locked);

	AttackShape.Type = ERSCombatShapeType::AnnularSector;
	AttackShape.InnerRadius = 0.0f;
	AttackShape.OuterRadius = 0.0f;
}

void URSGameplayAbility_Barrier_Collapse_Gimmick::CalculateBarrierZoneCenters(const FVector& PatternCenter, float DistanceFromCenterValue, TArray<FVector>& OutZoneCenters)
{
	OutZoneCenters.Reset();
	OutZoneCenters.Reserve(BarrierZoneCount);

	for (int32 ZoneIndex = 0; ZoneIndex < BarrierZoneCount; ++ZoneIndex)
	{
		const float AngleRadians = FMath::DegreesToRadians(FirstBarrierAngleDegrees + BarrierAngleStepDegrees * ZoneIndex);
		OutZoneCenters.Add(PatternCenter + FVector(FMath::Cos(AngleRadians), FMath::Sin(AngleRadians), 0.0f) * DistanceFromCenterValue);
	}
}

int32 URSGameplayAbility_Barrier_Collapse_Gimmick::FindSafeZoneIndex(TConstArrayView<FVector> ZoneCenters, const FVector& Location, float SafeRadiusValue)
{
	// 인접한 방어막이 겹치지 않도록 검증하므로 위치를 포함하는 방어막은 최대 하나입니다
	for (int32 ZoneIndex = 0; ZoneIndex < ZoneCenters.Num(); ++ZoneIndex)
	{
		if (URSGameplayAbility_Barrier_Memory_Gimmick::IsLocationInsideSafeZone(Location, ZoneCenters[ZoneIndex], SafeRadiusValue))
		{
			return ZoneIndex;
		}
	}

	return INDEX_NONE;
}

void URSGameplayAbility_Barrier_Collapse_Gimmick::ResolveHitCheck(TConstArrayView<FVector> ZoneCenters, TConstArrayView<FVector> TargetLocations, float SafeRadiusValue, TArray<int32>& OutDestroyedZoneIndices, TArray<int32>& OutFailedTargetIndices)
{
	OutDestroyedZoneIndices.Reset();
	OutFailedTargetIndices.Reset();

	for (int32 TargetIndex = 0; TargetIndex < TargetLocations.Num(); ++TargetIndex)
	{
		const int32 SafeZoneIndex = FindSafeZoneIndex(ZoneCenters, TargetLocations[TargetIndex], SafeRadiusValue);
		if (SafeZoneIndex == INDEX_NONE)
		{
			OutFailedTargetIndices.Add(TargetIndex);

			continue;
		}

		// 같은 방어막에 여럿이 들어가 있어도 파괴는 한 번이므로 중복을 남기지 않습니다
		OutDestroyedZoneIndices.AddUnique(SafeZoneIndex);
	}

	OutDestroyedZoneIndices.Sort();
}

FTransform URSGameplayAbility_Barrier_Collapse_Gimmick::MakeAttackBeamPatternTransform(const FTransform& PatternTransform, const FVector& CurrentForward)
{
	FVector HorizontalForward = CurrentForward;
	HorizontalForward.Z = 0.0f;

	// Beam을 생성하지 못해도 패턴은 계속 진행하므로, 전방을 얻지 못한 경우에는 시작 회전을 그대로 두고 위치만 유지합니다
	if (!HorizontalForward.Normalize())
	{
		return PatternTransform;
	}

	return FTransform(HorizontalForward.Rotation(), PatternTransform.GetLocation());
}

bool URSGameplayAbility_Barrier_Collapse_Gimmick::IsBarrierFieldValid(float DistanceFromCenterValue, float SafeRadiusValue, FString* OutValidationError)
{
	auto SetError = [OutValidationError](const FString& Message)
	{
		if (OutValidationError)
		{
			*OutValidationError = Message;
		}
	};

	if (!FMath::IsFinite(DistanceFromCenterValue) || DistanceFromCenterValue <= 0.0f || !FMath::IsFinite(SafeRadiusValue) || SafeRadiusValue <= 0.0f)
	{
		SetError(TEXT("DistanceFromCenter and SafeRadius must be finite and positive."));

		return false;
	}

	// 인접한 두 방어막 중심은 DistanceFromCenter * sqrt(2)만큼 떨어지므로, 반지름이 그 절반에 닿으면 한 플레이어가 두 곳에 동시에 들어가 파괴 대상이 모호해집니다
	if (SafeRadiusValue >= DistanceFromCenterValue / FMath::Sqrt(2.0f))
	{
		SetError(TEXT("SafeRadius must be smaller than DistanceFromCenter / sqrt(2)."));

		return false;
	}

	return true;
}

void URSGameplayAbility_Barrier_Collapse_Gimmick::BeginPatternTimeline()
{
	bIsEndingAbility = false;

	EndGameplayEventTasks();
	DestroyBarrierZones();
	PatternCenterTransform = FTransform::Identity;
	PreviewEventCount = 0;
	AttackIndex = 0;
	bReceivedAttackPresentation = false;
	bReceivedHitCheck = false;

	const AActor* AvatarActor = CurrentActorInfo ? CurrentActorInfo->AvatarActor.Get() : nullptr;
	FString RuntimeValidationError;
	const bool bHasValidAttackShape = AttackShape.Type == ERSCombatShapeType::AnnularSector && AttackShape.InnerRadius == 0.0f && AttackShape.IsDataValid(&RuntimeValidationError)
		&& AttackShape.OuterRadius >= DistanceFromCenter + SafeRadius;
	if (!AvatarActor || !CurrentActorInfo->AbilitySystemComponent.IsValid() || !RoarMontage || !AttackMontage
		|| !FMath::IsFinite(RoarMontagePlayRate) || RoarMontagePlayRate <= 0.0f || !FMath::IsFinite(AttackMontagePlayRate) || AttackMontagePlayRate <= 0.0f
		|| !FMath::IsFinite(AimRotationSpeed) || AimRotationSpeed <= 0.0f || !FMath::IsFinite(AimYawTolerance) || AimYawTolerance < 0.0f
		|| !FMath::IsFinite(TargetDriftTolerance) || TargetDriftTolerance < 0.0f || !FMath::IsFinite(MaxAimDuration) || MaxAimDuration <= 0.0f
		|| !IsBarrierFieldValid(DistanceFromCenter, SafeRadius, &RuntimeValidationError) || !bHasValidAttackShape)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	if (!CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	// 방어막과 공격 연출이 보스 발밑 평면에 놓이도록 액터 원점이 아니라 캡슐 바닥을 패턴 원점으로 씁니다
	FVector PatternCenterLocation = FVector::ZeroVector;
	if (!URSCombatFunctionLibrary::TryGetActorGroundLocation(AvatarActor, PatternCenterLocation))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	// 공격마다 보스가 타겟을 향해 돌기 때문에 여기 저장하는 회전은 전방을 얻지 못했을 때 Beam이 쓸 대비값입니다. 방어막 각도는 이 회전을 쓰지 않아 월드 기준을 유지합니다
	FVector HorizontalForward = AvatarActor->GetActorForwardVector();
	HorizontalForward.Z = 0.0f;
	if (!HorizontalForward.Normalize())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

		return;
	}

	PatternCenterTransform = FTransform(HorizontalForward.Rotation(), PatternCenterLocation);

	CreateBarrierField();
	if (!CreateBarrierPresentations())
	{
		CancelForInvalidRuntime(TEXT("could not create all barrier presentations"));

		return;
	}

	DrawDebugBarrierZones(BarrierFieldDebugLifeTime);
	StartPreviewMontage();
}

void URSGameplayAbility_Barrier_Collapse_Gimmick::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
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

void URSGameplayAbility_Barrier_Collapse_Gimmick::CreateBarrierField()
{
	TArray<FVector> ZoneCenters;
	CalculateBarrierZoneCenters(PatternCenterTransform.GetLocation(), DistanceFromCenter, ZoneCenters);

	BarrierZones.Reset();
	BarrierZones.Reserve(ZoneCenters.Num());

	for (const FVector& ZoneCenter : ZoneCenters)
	{
		BarrierZones.AddDefaulted_GetRef().Center = ZoneCenter;
	}
}

bool URSGameplayAbility_Barrier_Collapse_Gimmick::CreateBarrierPresentations()
{
	bool bCreatedAllPresentations = true;
	for (FRSBarrierCollapseZoneRuntimeState& Zone : BarrierZones)
	{
		Zone.Presentation = SpawnNiagaraFromDefinition(BarrierNiagara, FTransform(Zone.Center), false);
		bCreatedAllPresentations &= Zone.Presentation.IsValid();
	}

	return bCreatedAllPresentations;
}

void URSGameplayAbility_Barrier_Collapse_Gimmick::DestroyBarrierZonesAt(TConstArrayView<int32> ZoneIndices)
{
	// 오름차순 인덱스를 앞에서부터 지우면 뒤쪽 인덱스가 밀리므로 뒤에서부터 제거합니다
	for (int32 Cursor = ZoneIndices.Num() - 1; Cursor >= 0; --Cursor)
	{
		const int32 ZoneIndex = ZoneIndices[Cursor];
		if (!BarrierZones.IsValidIndex(ZoneIndex))
		{
			continue;
		}

		if (UNiagaraComponent* Presentation = BarrierZones[ZoneIndex].Presentation.Get())
		{
			Presentation->DestroyComponent();
		}

		BarrierZones.RemoveAt(ZoneIndex);
	}
}

void URSGameplayAbility_Barrier_Collapse_Gimmick::DestroyBarrierZones()
{
	for (FRSBarrierCollapseZoneRuntimeState& Zone : BarrierZones)
	{
		if (UNiagaraComponent* Presentation = Zone.Presentation.Get())
		{
			Presentation->DestroyComponent();
		}
	}

	BarrierZones.Reset();
}

void URSGameplayAbility_Barrier_Collapse_Gimmick::DrawDebugBarrierZones(float LifeTime) const
{
#if ENABLE_DRAW_DEBUG
	if (!URSCombatFunctionLibrary::IsHitCheckDebugEnabled())
	{
		return;
	}

	FRSCombatShape ZoneShape;
	ZoneShape.Type = ERSCombatShapeType::AnnularSector;
	ZoneShape.OuterRadius = SafeRadius;

	// 색 구분이 없고 파괴된 방어막은 목록에서 빠지므로, 남아 있는 원은 모두 지금 안전한 자리입니다
	for (const FRSBarrierCollapseZoneRuntimeState& Zone : BarrierZones)
	{
		URSCombatFunctionLibrary::DrawDebugCombatShape(GetWorld(), ZoneShape, FTransform(Zone.Center), FColor::Green, LifeTime);
	}
#endif
}

void URSGameplayAbility_Barrier_Collapse_Gimmick::StartPreviewMontage()
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

void URSGameplayAbility_Barrier_Collapse_Gimmick::StartAttackMontage()
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

	StartAttackFacing();

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

void URSGameplayAbility_Barrier_Collapse_Gimmick::StartAttackFacing()
{
	ARSBossCharacter* BossCharacter = nullptr;
	ARSBossController* BossController = nullptr;
	if (!GetBossContext(BossCharacter, BossController))
	{
		return;
	}

	AActor* TargetActor = BossController->GetTargetActor();
	if (!BossController->IsTargetActorValid(TargetActor))
	{
		return;
	}

	const FRSBossFacingRequest FacingRequest = FRSBossFacingRequest::MakeTrackActor(TargetActor, AimRotationSpeed, AimYawTolerance, TargetDriftTolerance, MaxAimDuration);
	if (!FacingRequest.IsDataValid())
	{
		return;
	}

	// 회전은 판정이 아니라 가독성을 위한 것이므로 OnFacingFinished를 기다리지 않습니다
	// 기다리면 세 공격 앞에 조준 대기가 하나씩 붙어 준비 동작으로 확보한 대응 시간이 흐트러집니다
	URSAbilityTask_BossFacing* FacingTask = CreateBossFacingTask(FacingRequest);
	if (!FacingTask)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s could not create a facing task for attack %d"), *GetName(), AttackIndex);

		return;
	}

	FacingTask->ReadyForActivation();
}

bool URSGameplayAbility_Barrier_Collapse_Gimmick::GetBossContext(ARSBossCharacter*& OutBossCharacter, ARSBossController*& OutBossController) const
{
	OutBossCharacter = CurrentActorInfo ? Cast<ARSBossCharacter>(CurrentActorInfo->AvatarActor.Get()) : nullptr;
	OutBossController = OutBossCharacter ? Cast<ARSBossController>(OutBossCharacter->GetController()) : nullptr;

	return OutBossCharacter && OutBossController;
}

void URSGameplayAbility_Barrier_Collapse_Gimmick::EndGameplayEventTasks()
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

void URSGameplayAbility_Barrier_Collapse_Gimmick::HandleEyeNiagaraEvent(FGameplayEventData Payload)
{
	if (PreviewEventCount >= PatternAttackCount)
	{
		CancelForInvalidRuntime(TEXT("received too many preview presentation events"));

		return;
	}

	// 색 예고가 없어도 안광은 남은 공격 횟수를 알리는 유일한 표현이므로 생성하지 못하면 진행하지 않습니다
	if (!SpawnNiagaraFromDefinition(EyeNiagara, PatternCenterTransform, true))
	{
		CancelForInvalidRuntime(TEXT("could not create an eye presentation"));

		return;
	}

	++PreviewEventCount;
}

void URSGameplayAbility_Barrier_Collapse_Gimmick::HandleSwordNiagaraEvent(FGameplayEventData Payload)
{
	if (bReceivedAttackPresentation || bReceivedHitCheck)
	{
		CancelForInvalidRuntime(TEXT("received an extra or out-of-order attack presentation event"));

		return;
	}

	if (!SpawnNiagaraFromDefinition(SwordNiagara, PatternCenterTransform, true))
	{
		CancelForInvalidRuntime(TEXT("could not create a sword presentation"));

		return;
	}

	bReceivedAttackPresentation = true;
}

void URSGameplayAbility_Barrier_Collapse_Gimmick::HandleHitCheckEvent(FGameplayEventData Payload)
{
	if (!bReceivedAttackPresentation || bReceivedHitCheck)
	{
		CancelForInvalidRuntime(TEXT("received an extra or out-of-order hit check event"));

		return;
	}

	bReceivedHitCheck = true;
	DrawDebugBarrierZones(BarrierHitCheckDebugLifeTime);
	PlayBeamPresentation();
	ExecuteCurrentHitCheck();
}

void URSGameplayAbility_Barrier_Collapse_Gimmick::PlayBeamPresentation()
{
	// 보스가 공격마다 타겟을 향해 돌기 때문에 Beam도 시작 방향이 아니라 지금 보고 있는 방향으로 나가야 칼과 따로 놀지 않습니다
	const AActor* AvatarActor = CurrentActorInfo ? CurrentActorInfo->AvatarActor.Get() : nullptr;
	const FVector CurrentForward = AvatarActor ? AvatarActor->GetActorForwardVector() : FVector::ZeroVector;
	const FTransform BeamPatternTransform = MakeAttackBeamPatternTransform(PatternCenterTransform, CurrentForward);

	// 이미 보이는 공격에 얹는 연출이고 이 시점에는 피해가 나가는 중이므로, 생성하지 못해도 패턴을 취소하지 않고 기록만 남깁니다
	if (!SpawnNiagaraFromDefinition(BeamNiagara, URSGameplayAbility_Barrier_Memory_Gimmick::CalculateBeamTransform(BeamPatternTransform, BeamOffset), true))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s could not create a beam presentation for attack %d"), *GetName(), AttackIndex);
	}
}

void URSGameplayAbility_Barrier_Collapse_Gimmick::HandlePreviewMontageCompleted()
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

void URSGameplayAbility_Barrier_Collapse_Gimmick::HandleAttackMontageCompleted()
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
		EndActiveBossFacing();
		DestroyBarrierZones();
		FinishPatternWhenMontageEnds();

		return;
	}

	StartAttackMontage();
}

void URSGameplayAbility_Barrier_Collapse_Gimmick::HandleMontageCancelled()
{
	if (!bIsEndingAbility && IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

void URSGameplayAbility_Barrier_Collapse_Gimmick::ExecuteCurrentHitCheck()
{
	AActor* AvatarActor = CurrentActorInfo ? CurrentActorInfo->AvatarActor.Get() : nullptr;
	if (!AvatarActor)
	{
		CancelForInvalidRuntime(TEXT("lost its avatar before hit check"));

		return;
	}

	TArray<AActor*> HitTargets;
	URSCombatFunctionLibrary::FindTargetsInShape(AvatarActor, TargetChannel, AttackShape, PatternCenterTransform, HitTargets);

	TArray<ARSPlayerCharacter*> PlayerTargets;
	TArray<FVector> TargetLocations;
	for (AActor* HitTarget : HitTargets)
	{
		if (ARSPlayerCharacter* PlayerCharacter = Cast<ARSPlayerCharacter>(HitTarget))
		{
			PlayerTargets.Add(PlayerCharacter);
			TargetLocations.Add(PlayerCharacter->GetActorLocation());
		}
	}

	TArray<FVector> ZoneCenters;
	ZoneCenters.Reserve(BarrierZones.Num());
	for (const FRSBarrierCollapseZoneRuntimeState& Zone : BarrierZones)
	{
		ZoneCenters.Add(Zone.Center);
	}

	TArray<int32> DestroyedZoneIndices;
	TArray<int32> FailedTargetIndices;
	ResolveHitCheck(ZoneCenters, TargetLocations, SafeRadius, DestroyedZoneIndices, FailedTargetIndices);

	for (const int32 FailedTargetIndex : FailedTargetIndices)
	{
		ApplyPatternHitToTarget(PlayerTargets[FailedTargetIndex]);
	}

	DestroyBarrierZonesAt(DestroyedZoneIndices);
}

void URSGameplayAbility_Barrier_Collapse_Gimmick::CancelForInvalidRuntime(const TCHAR* Reason)
{
	UE_LOG(LogTemp, Warning, TEXT("%s cancelled because it %s"), *GetName(), Reason);

	if (!bIsEndingAbility && IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

#if WITH_EDITOR
EDataValidationResult URSGameplayAbility_Barrier_Collapse_Gimmick::IsDataValid(FDataValidationContext& Context) const
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

	if (!FMath::IsFinite(AimRotationSpeed) || AimRotationSpeed <= 0.0f || !FMath::IsFinite(AimYawTolerance) || AimYawTolerance < 0.0f
		|| !FMath::IsFinite(TargetDriftTolerance) || TargetDriftTolerance < 0.0f || !FMath::IsFinite(MaxAimDuration) || MaxAimDuration <= 0.0f)
	{
		AddError(TEXT("Aim values must be finite, with positive AimRotationSpeed and MaxAimDuration and non-negative tolerances."));
	}

	FString BarrierError;
	if (!IsBarrierFieldValid(DistanceFromCenter, SafeRadius, &BarrierError))
	{
		AddError(FString::Printf(TEXT("Barrier field is invalid: %s"), *BarrierError));
	}

	FString ShapeError;
	// 방어막 판정은 보스를 중심으로 모든 방향을 덮어야 하므로 부채꼴도 도넛도 허용하지 않습니다
	if (AttackShape.Type != ERSCombatShapeType::AnnularSector || !AttackShape.IsDataValid(&ShapeError)
		|| AttackShape.InnerRadius != 0.0f || !AttackShape.GetAnnularSectorBounds().CoversEveryAngle())
	{
		AddError(FString::Printf(TEXT("AttackShape must be a valid full circle Annular Sector with InnerRadius 0: %s"), *ShapeError));
	}
	else if (AttackShape.OuterRadius < DistanceFromCenter + SafeRadius)
	{
		AddError(TEXT("AttackShape.OuterRadius must cover DistanceFromCenter + SafeRadius."));
	}

	const TPair<const TCHAR*, const FRSNiagaraSpawnDefinition*> NiagaraDefinitions[] =
	{
		{ TEXT("BarrierNiagara"), &BarrierNiagara },
		{ TEXT("EyeNiagara"), &EyeNiagara },
		{ TEXT("SwordNiagara"), &SwordNiagara },
		{ TEXT("BeamNiagara"), &BeamNiagara }
	};

	for (const TPair<const TCHAR*, const FRSNiagaraSpawnDefinition*>& Pair : NiagaraDefinitions)
	{
		FString DefinitionError;
		if (!Pair.Value->IsDataValid(&DefinitionError))
		{
			AddError(FString::Printf(TEXT("%s is invalid: %s"), Pair.Key, *DefinitionError));
		}
	}

	if (EyeNiagara.SpawnMode == ERSNiagaraSpawnMode::WorldTransform || SwordNiagara.SpawnMode == ERSNiagaraSpawnMode::WorldTransform)
	{
		AddError(TEXT("Eye and sword Niagara definitions must use a socket spawn mode."));
	}

	if (BarrierNiagara.SpawnMode != ERSNiagaraSpawnMode::WorldTransform)
	{
		AddError(TEXT("BarrierNiagara must use WorldTransform."));
	}

	// Beam은 소켓이 아니라 패턴이 고정한 전방 위치에 놓이므로 소켓 기준 생성은 BeamOffset을 무시하게 됩니다
	if (BeamNiagara.SpawnMode != ERSNiagaraSpawnMode::WorldTransform)
	{
		AddError(TEXT("BeamNiagara must use WorldTransform."));
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

	// 순서 암기 기믹과 같은 주기 피해 Effect를 쓰므로 같은 계약을 검사합니다
	const UGameplayEffect* DamageEffect = HitDefinition.DamageEffectClass ? HitDefinition.DamageEffectClass->GetDefaultObject<UGameplayEffect>() : nullptr;
	if (DamageEffect)
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
