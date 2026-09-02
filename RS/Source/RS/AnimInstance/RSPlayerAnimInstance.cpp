// Fill out your copyright notice in the Description page of Project Settings.


#include "RSPlayerAnimInstance.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"

void URSPlayerAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	// 매 프레임 소유 객체를 다시 찾지 않도록 애님 인스턴스의 폰과 이동 컴포넌트를 캐싱한다
	// 캐릭터가 아닌 폰이나 에디터 프리뷰에서는 이동 컴포넌트가 없을 수 있다
	PlayerPawn = TryGetPawnOwner();
	PlayerMovement = PlayerPawn ? Cast<UCharacterMovementComponent>(PlayerPawn->GetMovementComponent()) : nullptr;
}

void URSPlayerAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);

	// 에디터 프리뷰 또는 객체가 제거되는 도중에도 애님 그래프에 이전 값이 남지 않도록 초기화한다
	if (!PlayerPawn || !PlayerMovement)
	{
		GroundVelocity = FVector::ZeroVector;
		GroundSpeed = 0.0f;
		bShouldMove = false;
		bIsFalling = false;
		return;
	}

	const FVector Velocity = PlayerPawn->GetVelocity();

	// 점프와 낙하의 Z축 속도는 지상 이동 Blend Space에 영향을 주지 않도록 제외한다
	GroundVelocity = FVector(Velocity.X, Velocity.Y, 0.0f);
	GroundSpeed = GroundVelocity.Size();

	// 작은 속도 오차는 무시하고, 입력 가속도가 있을 때만 이동 애니메이션으로 전환한다
	// 따라서 입력을 놓고 감속만 진행 중일 때 이동 상태가 불필요하게 유지되지 않는다
	bShouldMove = GroundSpeed > 3.0f;

	// 점프뿐 아니라 지면에서 떨어져 낙하하는 상태도 포함한다
	bIsFalling = PlayerMovement->IsFalling();
}
