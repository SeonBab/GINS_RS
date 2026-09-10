// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/** 전투 공간에서 사용할 무작위 위치 표본 추출 함수를 제공합니다 */
namespace RSRandomPointSampling
{
	/** 원점 중심 Annulus 면적에 균일한 무작위 수평 오프셋을 생성합니다 */
	RS_API bool TrySamplePointInAnnulus(float MinRadius, float MaxRadius, FRandomStream& InOutRandomStream, FVector2D& OutOffset);
}
