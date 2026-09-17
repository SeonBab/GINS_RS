// Fill out your copyright notice in the Description page of Project Settings.

#include "RSInteractionSelection.h"

#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "RSInteractable.h"

namespace RSInteraction
{
	namespace
	{
		/**
		 * 기준 위치에서 형상 표면까지의 거리를 반환하며 단순 충돌이 없으면 컴포넌트 위치까지의 거리를 사용합니다
		 * 형상 질의가 실패했다고 후보에서 빼면 대상이 조용히 사라지므로 덜 정확한 값으로라도 순위를 매깁니다
		 */
		float GetDistanceToShape(const UPrimitiveComponent& CandidateComponent, const FVector& Origin)
		{
			FVector ClosestPoint = FVector::ZeroVector;
			const float DistanceToShape = CandidateComponent.GetClosestPointOnCollision(Origin, ClosestPoint);

			if (DistanceToShape >= 0.0f)
			{
				return DistanceToShape;
			}

			return FVector::Dist(Origin, CandidateComponent.GetComponentLocation());
		}
	}

	AActor* FindNearestInteractable(TConstArrayView<UPrimitiveComponent*> CandidateComponents, const FVector& Origin, const AActor* InteractInstigator)
	{
		AActor* NearestInteractable = nullptr;
		float NearestDistance = TNumericLimits<float>::Max();

		for (UPrimitiveComponent* CandidateComponent : CandidateComponents)
		{
			if (!IsValid(CandidateComponent))
			{
				continue;
			}

			AActor* CandidateActor = CandidateComponent->GetOwner();
			if (!IsValid(CandidateActor) || CandidateActor == InteractInstigator)
			{
				continue;
			}

			if (!CandidateActor->Implements<URSInteractable>())
			{
				continue;
			}

			if (!IRSInteractable::Execute_CanInteract(CandidateActor, InteractInstigator))
			{
				continue;
			}

			const float DistanceToShape = GetDistanceToShape(*CandidateComponent, Origin);
			if (DistanceToShape >= NearestDistance)
			{
				continue;
			}

			NearestDistance = DistanceToShape;
			NearestInteractable = CandidateActor;
		}

		return NearestInteractable;
	}
}
