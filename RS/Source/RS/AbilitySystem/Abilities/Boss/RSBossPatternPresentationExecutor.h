// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

class UNiagaraComponent;
class UObject;
class USkeletalMeshComponent;
struct FRSBossPatternNiagaraEntry;
struct FRSBossPatternPresentation;
struct FRSCombatShape;
struct FRSNiagaraSpawnDefinition;

/** 보스 패턴의 설정을 받아 Niagara, Sound와 카메라 셰이크를 상태 없이 실행합니다 */
class RS_API FRSBossPatternPresentationExecutor
{
public:
	/** 형상과 기준 Transform을 연출 설정에 따라 배치하고 Sound와 카메라 셰이크를 한 번씩 재생합니다 */
	static void Play(const UObject* WorldContextObject, USkeletalMeshComponent* MeshComponent, const FRSBossPatternPresentation& Presentation, TArrayView<const FRSCombatShape> HitShapes, TArrayView<const FTransform> PresentationTransforms, const FVector& SoundLocation);

	/** 정의가 지정한 기준으로 Niagara를 생성합니다 */
	static UNiagaraComponent* SpawnNiagaraFromDefinition(const UObject* WorldContextObject, USkeletalMeshComponent* MeshComponent, const FRSNiagaraSpawnDefinition& Definition, const FTransform& WorldTransform, bool bAutoDestroy);

	/** 월드 Transform을 한 Niagara Component 기준의 로컬 배열과 Fixed Bounds로 변환합니다 */
	static bool BuildNiagaraTransformArrays(TArrayView<const FTransform> WorldTransforms, const FTransform& SystemTransform, float BoundsPadding, TArray<FVector>& OutPositions, TArray<FQuat>& OutRotations, TArray<FVector>& OutScales, FBox& OutLocalBounds);

#if WITH_EDITOR
	/** 연출 설정이 넘긴 판정 형상을 지원하고 생성 상한을 지키는지 검사합니다 */
	static EDataValidationResult Validate(const FRSBossPatternPresentation& Presentation, const FRSCombatShape& HitShape, FDataValidationContext& Context);
#endif

private:
	/** 연출 항목의 배치 규칙으로 계산한 모든 Transform을 선택한 생성 방식으로 재생합니다 */
	static void PlayNiagaraEntry(const UObject* WorldContextObject, USkeletalMeshComponent* MeshComponent, const FRSBossPatternNiagaraEntry& NiagaraEntry, TArrayView<const FTransform> WorldTransforms, const FVector& SystemLocation);

	/** 한 Component에 로컬 위치, 회전과 스케일 배열을 설정한 뒤 활성화합니다 */
	static void SpawnPositionArrayNiagaraSystem(const UObject* WorldContextObject, const FRSBossPatternNiagaraEntry& NiagaraEntry, TArrayView<const FTransform> WorldTransforms, const FVector& SystemLocation);
};
