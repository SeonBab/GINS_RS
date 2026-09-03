// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RSPlayerController.generated.h"

class URSLocalPlayerViewModelSubsystem;
class URSPlayerCameraComponent;

/** 마우스 위치, 로컬 카메라 컴포넌트와 ViewModel 데이터 원본 연결을 관리합니다 */
UCLASS()
class RS_API ARSPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	/** 로컬 플레이어의 카메라 컴포넌트를 생성합니다 */
	ARSPlayerController();

protected:
	/** 로컬 입력을 준비하고 현재 Pawn을 ViewModel 데이터 원본으로 등록합니다 */
	virtual void BeginPlay() override;

	/** 현재 Pawn의 ViewModel 데이터 원본 등록을 해제합니다 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	/** Pawn이 변경되면 이전 데이터 원본을 새 Pawn으로 교체합니다 */
	virtual void SetPawn(APawn* InPawn) override;

	/** 매 프레임 기록된 어빌리티 입력을 처리합니다 */
	virtual void PostProcessInput(float DeltaTime, bool bGamePaused) override;

	/**
	 * 마우스 커서 아래의 유효한 월드 위치를 반환합니다
	 * 위치 조회만 담당하며 Character의 이동 또는 어빌리티 실행 상태를 변경하지 않습니다
	 */
	bool GetCursorWorldLocation(FVector& OutCursorWorldLocation) const;

	/** 로컬 플레이어의 카메라 상태와 ViewTarget 전환을 담당하는 컴포넌트를 반환합니다 */
	URSPlayerCameraComponent* GetPlayerCameraComponent() const;

	/** 로컬 ViewModel이 선택적으로 관찰할 게임 데이터 원본을 등록합니다 */
	void RegisterViewModelSource(UObject* Source);

	/** 로컬 ViewModel에 등록한 게임 데이터 원본을 해제합니다 */
	void UnregisterViewModelSource(UObject* Source);

private:
	/** 로컬 플레이어가 월드와 UI를 마우스로 조작할 수 있도록 커서와 입력 모드를 설정합니다 */
	void ConfigureMouseInput();

	/** 현재 로컬 플레이어의 ViewModel 저장소를 반환합니다 */
	URSLocalPlayerViewModelSubsystem* GetViewModelSubsystem() const;

private:
	/** 로컬 플레이어의 카메라 상태와 전투 CameraActor 수명을 관리합니다 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URSPlayerCameraComponent> PlayerCameraComp;
};
