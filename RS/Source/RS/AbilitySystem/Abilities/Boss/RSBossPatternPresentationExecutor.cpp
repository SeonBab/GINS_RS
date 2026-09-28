// Fill out your copyright notice in the Description page of Project Settings.

#include "RSBossPatternPresentationExecutor.h"

#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraDataInterfaceArrayFloat.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "RSBaseGameplayAbility_BossPattern.h"

namespace
{
	// 배열 방식도 위치마다 파티클을 만들므로 설정 실수로 한 번의 연출이 렌더링을 압도하지 않게 막습니다
	constexpr int32 RSMaxNiagaraFillPositionCount = 256;
	const FName RSNiagaraFillPositionsParameter(TEXT("User.RSFillPositions"));
	const FName RSNiagaraFillRotationsParameter(TEXT("User.RSFillRotations"));
	const FName RSNiagaraFillScalesParameter(TEXT("User.RSFillScales"));

#if WITH_EDITOR
	bool HasNiagaraArrayParameter(const UNiagaraSystem* NiagaraSystem, UClass* DataInterfaceClass, FName ParameterName)
	{
		if (!NiagaraSystem || !DataInterfaceClass)
		{
			return false;
		}

		const FNiagaraVariable Parameter(FNiagaraTypeDefinition(DataInterfaceClass), ParameterName);

		return NiagaraSystem->GetExposedParameters().IndexOf(Parameter) != INDEX_NONE;
	}
#endif
}

void FRSBossPatternPresentationExecutor::Play(const UObject* WorldContextObject, USkeletalMeshComponent* MeshComponent, const FRSBossPatternPresentation& Presentation, TArrayView<const FRSCombatShape> HitShapes, TArrayView<const FTransform> PresentationTransforms, const FVector& SoundLocation)
{
	for (const FRSBossPatternNiagaraEntry& NiagaraEntry : Presentation.Niagaras)
	{
		if (!NiagaraEntry.Niagara.NiagaraSystem)
		{
			continue;
		}

		TArray<FTransform> NiagaraTransforms;

		// 형상을 넘기지 않는 패턴에서는 채울 범위가 없으므로 넘겨받은 지점을 그대로 사용합니다
		if (NiagaraEntry.Placement != ERSBossPatternNiagaraPlacement::FillHitShape || HitShapes.IsEmpty())
		{
			NiagaraTransforms.Reserve(PresentationTransforms.Num());
			for (const FTransform& PresentationTransform : PresentationTransforms)
			{
				NiagaraTransforms.Add(PresentationTransform);
			}
		}
		else
		{
			// 형상과 지점을 교차해 채우므로 형상 하나를 여러 지점에 두는 패턴과 한 지점에 형상 여러 개를 겹치는 패턴이 같은 경로를 씁니다
			for (const FRSCombatShape& HitShape : HitShapes)
			{
				for (const FTransform& PresentationTransform : PresentationTransforms)
				{
					TArray<FTransform> FillTransforms;
					if (!URSCombatFunctionLibrary::BuildShapeFillTransforms(HitShape, PresentationTransform, NiagaraEntry.FillSpacing, FillTransforms))
					{
						// Data Validation이 막지 못한 조합이라도 그 영역의 연출까지 사라지면 안 되므로 기준 지점 하나로 되돌립니다
						NiagaraTransforms.Add(PresentationTransform);

						continue;
					}

					NiagaraTransforms.Append(FillTransforms);
				}
			}
		}

		PlayNiagaraEntry(WorldContextObject, MeshComponent, NiagaraEntry, NiagaraTransforms, SoundLocation);
	}

	// 광역 패턴은 Niagara가 여러 지점에서 나지만 소리까지 겹쳐 울리면 한 번의 공격으로 들리지 않습니다
	if (Presentation.Sound)
	{
		UGameplayStatics::PlaySoundAtLocation(WorldContextObject, Presentation.Sound, SoundLocation);
	}

	URSCombatFunctionLibrary::PlayCameraShake(WorldContextObject, Presentation.CameraShake);
}

UNiagaraComponent* FRSBossPatternPresentationExecutor::SpawnNiagaraFromDefinition(const UObject* WorldContextObject, USkeletalMeshComponent* MeshComponent, const FRSNiagaraSpawnDefinition& Definition, const FTransform& WorldTransform, bool bAutoDestroy)
{
	return URSCombatFunctionLibrary::SpawnNiagaraFromDefinition(WorldContextObject, MeshComponent, Definition, WorldTransform, bAutoDestroy);
}

void FRSBossPatternPresentationExecutor::PlayNiagaraEntry(const UObject* WorldContextObject, USkeletalMeshComponent* MeshComponent, const FRSBossPatternNiagaraEntry& NiagaraEntry, TArrayView<const FTransform> WorldTransforms, const FVector& SystemLocation)
{
	if (WorldTransforms.IsEmpty())
	{
		return;
	}

	if (NiagaraEntry.SpawnPolicy == ERSBossPatternNiagaraSpawnPolicy::PositionArray)
	{
		SpawnPositionArrayNiagaraSystem(WorldContextObject, NiagaraEntry, WorldTransforms, SystemLocation);

		return;
	}

	for (const FTransform& WorldTransform : WorldTransforms)
	{
		SpawnNiagaraFromDefinition(WorldContextObject, MeshComponent, NiagaraEntry.Niagara, WorldTransform, true);
	}
}

void FRSBossPatternPresentationExecutor::SpawnPositionArrayNiagaraSystem(const UObject* WorldContextObject, const FRSBossPatternNiagaraEntry& NiagaraEntry, TArrayView<const FTransform> WorldTransforms, const FVector& SystemLocation)
{
	if (NiagaraEntry.Niagara.SpawnMode != ERSNiagaraSpawnMode::WorldTransform)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s skipped array Niagara %s because PositionArray requires WorldTransform spawn mode"), *GetNameSafe(WorldContextObject), *GetNameSafe(NiagaraEntry.Niagara.NiagaraSystem));

		return;
	}

	const FTransform SystemTransform(FQuat::Identity, SystemLocation, FVector::OneVector);
	TArray<FVector> LocalPositions;
	TArray<FQuat> LocalRotations;
	TArray<FVector> LocalScales;
	FBox LocalBounds(EForceInit::ForceInit);
	if (!BuildNiagaraTransformArrays(WorldTransforms, SystemTransform, NiagaraEntry.BoundsPadding, LocalPositions, LocalRotations, LocalScales, LocalBounds))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s skipped array Niagara %s because its transforms or BoundsPadding are invalid"), *GetNameSafe(WorldContextObject), *GetNameSafe(NiagaraEntry.Niagara.NiagaraSystem));

		return;
	}

	UNiagaraComponent* NiagaraComp = URSCombatFunctionLibrary::SpawnNiagaraFromDefinition(WorldContextObject, nullptr, NiagaraEntry.Niagara, SystemTransform, true, false);
	if (!NiagaraComp)
	{
		return;
	}

	const bool bHasPositions = UNiagaraFunctionLibrary::GetDataInterface<UNiagaraDataInterfaceArrayFloat3>(NiagaraComp, RSNiagaraFillPositionsParameter) != nullptr;
	const bool bHasRotations = UNiagaraFunctionLibrary::GetDataInterface<UNiagaraDataInterfaceArrayQuat>(NiagaraComp, RSNiagaraFillRotationsParameter) != nullptr;
	const bool bHasScales = UNiagaraFunctionLibrary::GetDataInterface<UNiagaraDataInterfaceArrayFloat3>(NiagaraComp, RSNiagaraFillScalesParameter) != nullptr;
	if (!bHasPositions || !bHasRotations || !bHasScales)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s skipped array Niagara %s because it must expose User.RSFillPositions, User.RSFillRotations and User.RSFillScales"), *GetNameSafe(WorldContextObject), *GetNameSafe(NiagaraEntry.Niagara.NiagaraSystem));
		NiagaraComp->DestroyComponent();

		return;
	}

	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(NiagaraComp, RSNiagaraFillPositionsParameter, LocalPositions);
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayQuat(NiagaraComp, RSNiagaraFillRotationsParameter, LocalRotations);
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(NiagaraComp, RSNiagaraFillScalesParameter, LocalScales);
	NiagaraComp->SetSystemFixedBounds(LocalBounds);
	NiagaraComp->Activate(true);
}

bool FRSBossPatternPresentationExecutor::BuildNiagaraTransformArrays(TArrayView<const FTransform> WorldTransforms, const FTransform& SystemTransform, float BoundsPadding, TArray<FVector>& OutPositions, TArray<FQuat>& OutRotations, TArray<FVector>& OutScales, FBox& OutLocalBounds)
{
	if (WorldTransforms.IsEmpty() || SystemTransform.ContainsNaN() || !FMath::IsFinite(BoundsPadding) || BoundsPadding < 0.0f)
	{
		return false;
	}

	TArray<FVector> LocalPositions;
	TArray<FQuat> LocalRotations;
	TArray<FVector> LocalScales;
	FBox LocalBounds(EForceInit::ForceInit);
	LocalPositions.Reserve(WorldTransforms.Num());
	LocalRotations.Reserve(WorldTransforms.Num());
	LocalScales.Reserve(WorldTransforms.Num());

	for (const FTransform& WorldTransform : WorldTransforms)
	{
		if (WorldTransform.ContainsNaN())
		{
			return false;
		}

		const FTransform LocalTransform = WorldTransform.GetRelativeTransform(SystemTransform);
		if (LocalTransform.ContainsNaN())
		{
			return false;
		}

		LocalPositions.Add(LocalTransform.GetLocation());
		LocalRotations.Add(LocalTransform.GetRotation());
		LocalScales.Add(LocalTransform.GetScale3D());
		LocalBounds += LocalTransform.GetLocation();
	}

	OutPositions = MoveTemp(LocalPositions);
	OutRotations = MoveTemp(LocalRotations);
	OutScales = MoveTemp(LocalScales);
	OutLocalBounds = LocalBounds.ExpandBy(BoundsPadding);

	return true;
}

#if WITH_EDITOR
EDataValidationResult FRSBossPatternPresentationExecutor::Validate(const FRSBossPatternPresentation& Presentation, const FRSCombatShape& HitShape, FDataValidationContext& Context)
{
	EDataValidationResult ValidationResult = EDataValidationResult::Valid;

	// 항목마다 칸을 따로 채우므로 한 항목이 상한을 넘지 않아도 합쳐서 넘을 수 있습니다
	int32 TotalFillCount = 0;
	for (int32 EntryIndex = 0; EntryIndex < Presentation.Niagaras.Num(); ++EntryIndex)
	{
		const FRSBossPatternNiagaraEntry& NiagaraEntry = Presentation.Niagaras[EntryIndex];
		if (NiagaraEntry.SpawnPolicy == ERSBossPatternNiagaraSpawnPolicy::PositionArray)
		{
			if (NiagaraEntry.Niagara.SpawnMode != ERSNiagaraSpawnMode::WorldTransform)
			{
				Context.AddError(FText::FromString(FString::Printf(TEXT("PatternPresentation.Niagaras[%d] must use the WorldTransform spawn mode with the PositionArray spawn policy."), EntryIndex)));
				ValidationResult = EDataValidationResult::Invalid;
			}

			if (!FMath::IsFinite(NiagaraEntry.BoundsPadding) || NiagaraEntry.BoundsPadding < 0.0f)
			{
				Context.AddError(FText::FromString(FString::Printf(TEXT("PatternPresentation.Niagaras[%d].BoundsPadding must be finite and non-negative."), EntryIndex)));
				ValidationResult = EDataValidationResult::Invalid;
			}

			if (NiagaraEntry.Niagara.NiagaraSystem)
			{
				const bool bHasPositions = HasNiagaraArrayParameter(NiagaraEntry.Niagara.NiagaraSystem, UNiagaraDataInterfaceArrayFloat3::StaticClass(), RSNiagaraFillPositionsParameter);
				const bool bHasRotations = HasNiagaraArrayParameter(NiagaraEntry.Niagara.NiagaraSystem, UNiagaraDataInterfaceArrayQuat::StaticClass(), RSNiagaraFillRotationsParameter);
				const bool bHasScales = HasNiagaraArrayParameter(NiagaraEntry.Niagara.NiagaraSystem, UNiagaraDataInterfaceArrayFloat3::StaticClass(), RSNiagaraFillScalesParameter);
				if (!bHasPositions || !bHasRotations || !bHasScales)
				{
					Context.AddError(FText::FromString(FString::Printf(TEXT("PatternPresentation.Niagaras[%d] uses PositionArray, so its Niagara System must expose User.RSFillPositions, User.RSFillRotations and User.RSFillScales."), EntryIndex)));
					ValidationResult = EDataValidationResult::Invalid;
				}
			}
		}

		if (NiagaraEntry.Placement != ERSBossPatternNiagaraPlacement::FillHitShape)
		{
			continue;
		}

		// 채우기는 칸마다 다른 월드 위치를 써야 의미가 있고, 소켓 기준 생성은 모든 칸을 한 지점에 겹쳐 버립니다
		if (NiagaraEntry.Niagara.NiagaraSystem && NiagaraEntry.Niagara.SpawnMode != ERSNiagaraSpawnMode::WorldTransform)
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("PatternPresentation.Niagaras[%d] must use the WorldTransform spawn mode so every fill transform keeps its world placement."), EntryIndex)));
			ValidationResult = EDataValidationResult::Invalid;
		}

		if (!FMath::IsFinite(NiagaraEntry.FillSpacing) || NiagaraEntry.FillSpacing <= 0.0f)
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("PatternPresentation.Niagaras[%d].FillSpacing must be finite and greater than zero."), EntryIndex)));
			ValidationResult = EDataValidationResult::Invalid;

			continue;
		}

		if (!HitShape.IsDataValid())
		{
			continue;
		}

		// 생성 개수는 형상과 간격만으로 정해지므로 런타임에 칸을 버리지 않고 여기서 막습니다
		TArray<FTransform> FillTransformProbe;
		if (!URSCombatFunctionLibrary::BuildShapeFillTransforms(HitShape, FTransform::Identity, NiagaraEntry.FillSpacing, FillTransformProbe))
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("PatternPresentation.Niagaras[%d].FillSpacing cannot fill the attack shape. Widen the spacing, or use the Ability Defined placement for a shape the fill does not support."), EntryIndex)));
			ValidationResult = EDataValidationResult::Invalid;

			continue;
		}

		TotalFillCount += FillTransformProbe.Num();
	}

	if (TotalFillCount > RSMaxNiagaraFillPositionCount)
	{
		Context.AddError(FText::FromString(FString::Printf(TEXT("The attack shape and PatternPresentation.Niagaras would create %d Niagara fill positions in total and the limit is %d. Increase the fill spacing, reduce the attack shape, or remove a filled entry."), TotalFillCount, RSMaxNiagaraFillPositionCount)));
		ValidationResult = EDataValidationResult::Invalid;
	}

	return ValidationResult;
}
#endif
