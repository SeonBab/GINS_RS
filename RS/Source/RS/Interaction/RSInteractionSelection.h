// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

class UPrimitiveComponent;

namespace RSInteraction
{
	/**
	 * 후보 컴포넌트 중 형상 표면이 기준 위치에 가장 가까운 상호작용 대상을 반환하며 후보가 없으면 nullptr을 반환합니다
	 * 한 액터가 콜리전을 여러 개 가지면 그중 가장 가까운 값만 남으므로 호출자가 액터 중복을 따로 제거하지 않아도 됩니다
	 * Overlap 질의와 분리해 두어 물리 없이도 선정 규칙만 검증할 수 있습니다
	 */
	RS_API AActor* FindNearestInteractable(TConstArrayView<UPrimitiveComponent*> CandidateComponents, const FVector& Origin, const AActor* InteractInstigator);
}
