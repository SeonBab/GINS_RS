// Fill out your copyright notice in the Description page of Project Settings.

#include "RSRandomPointSampling.h"

bool RSRandomPointSampling::TrySampleRadiusInAnnulus(float MinRadius, float MaxRadius, FRandomStream& InOutRandomStream, float& OutRadius)
{
	OutRadius = 0.0f;

	if (!FMath::IsFinite(MinRadius)
		|| !FMath::IsFinite(MaxRadius)
		|| MinRadius < 0.0f
		|| MaxRadius <= MinRadius)
	{
		return false;
	}

	// 반지름이 아니라 반지름의 제곱을 균일하게 뽑아 Annulus 면적에 대한 밀도를 일정하게 합니다
	const float RadiusSquared = FMath::Lerp(FMath::Square(MinRadius), FMath::Square(MaxRadius), InOutRandomStream.FRand());
	OutRadius = FMath::Sqrt(RadiusSquared);

	return true;
}

bool RSRandomPointSampling::TrySamplePointInAnnulus(float MinRadius, float MaxRadius, FRandomStream& InOutRandomStream, FVector2D& OutOffset)
{
	OutOffset = FVector2D::ZeroVector;

	float Radius = 0.0f;
	if (!TrySampleRadiusInAnnulus(MinRadius, MaxRadius, InOutRandomStream, Radius))
	{
		return false;
	}

	const float AngleRadians = InOutRandomStream.FRandRange(0.0f, UE_TWO_PI);

	OutOffset = FVector2D(FMath::Cos(AngleRadians) * Radius, FMath::Sin(AngleRadians) * Radius);

	return true;
}
