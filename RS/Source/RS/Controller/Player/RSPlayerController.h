// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RSBossResultAction.h"
#include "RSPlayerController.generated.h"

class URSLocalPlayerViewModelSubsystem;
class URSInGameMenuWidget;
class URSPlayerCameraComponent;
class UInputAction;
enum class ERSBossEncounterResult : uint8;
enum class ERSInGameMenuAction : uint8;

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

	/** PlayerController가 소유하는 사용자 인터페이스 입력을 연결합니다 */
	virtual void SetupInputComponent() override;

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

	/** 확정된 Boss 결과와 완료 Snapshot을 후속 Local Presentation이 소비할 수 있도록 한 번만 기록합니다 */
	void BeginBossResultPresentation(ERSBossEncounterResult Result, float RemainingTimeSeconds, int32 HitCount);

	/** Boss Result Presentation 진입 요청을 이미 받았는지 반환합니다 */
	bool HasBossResultPresentationStarted() const { return BossResultPresentation.IsSet(); }

	/** Local Presentation에 전달된 Boss Encounter 결과를 반환합니다 */
	ERSBossEncounterResult GetBossResultPresentation() const;

	/** Local Presentation에 전달된 완료 순간 남은 시간을 반환합니다 */
	float GetBossResultRemainingTimeSeconds() const { return BossResultRemainingTimeSeconds; }

	/** Local Presentation에 전달된 완료 순간 피격 횟수를 반환합니다 */
	int32 GetBossResultHitCount() const { return BossResultHitCount; }

	/** Local Result UI의 Action 의도를 현재 World의 Game Rule 계층으로 전달합니다 */
	bool RequestBossResultAction(ERSBossResultAction Action);

	/** GameMode가 Action을 승인하면 Local Result UI의 추가 입력을 비활성화합니다 */
	void HandleBossResultActionAccepted();

	/** 인게임 메뉴를 표시하고 World와 입력을 Pause 상태로 전환합니다 */
	bool OpenInGameMenu();

	/** 인게임 메뉴를 숨기고 열기 전 Pause와 게임플레이 입력 상태를 복원합니다 */
	bool CloseInGameMenu();

	/** 인게임 메뉴가 현재 열려 있는지 반환합니다 */
	bool IsInGameMenuOpen() const { return bIsInGameMenuOpen; }

	/** 인게임 메뉴의 Level 전환 의도를 현재 World의 GameMode에 전달합니다 */
	bool RequestInGameMenuAction(ERSInGameMenuAction Action);

	/** GameMode가 Level 전환을 승인하면 인게임 메뉴의 추가 입력을 비활성화합니다 */
	void HandleInGameMenuActionAccepted();

	/** 화면 전환 연출을 요청하고 연출이 끝난 뒤 실행할 동작을 HUD에 전달합니다 */
	bool PlayScreenTransition(const FSimpleDelegate& OnFadedOut);

private:
	/** 로컬 플레이어가 월드와 UI를 마우스로 조작할 수 있도록 커서와 입력 모드를 설정합니다 */
	void ConfigureMouseInput();

	/** Pause 상태에서 인게임 메뉴만 입력과 Focus를 받도록 설정합니다 */
	void ConfigureInGameMenuInput(URSInGameMenuWidget* InGameMenuWidget);

	/** 인게임 메뉴가 열려 있으면 닫고 닫혀 있으면 엽니다 */
	void Input_ToggleInGameMenu();

	/** 현재 로컬 플레이어의 ViewModel 저장소를 반환합니다 */
	URSLocalPlayerViewModelSubsystem* GetViewModelSubsystem() const;

private:
	friend class FRSBossEncounterStateTest;

	/** 로컬 플레이어의 카메라 상태와 전투 CameraActor 수명을 관리합니다 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URSPlayerCameraComponent> PlayerCameraComp;

	/** 일반 플레이와 Pause 상태에서 인게임 메뉴를 Toggle하는 Input Action입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> InGameMenuInputAction;

	/** 후속 Presentation 정책과 분리해 보존하는 최초 Boss Encounter 결과입니다 */
	TOptional<ERSBossEncounterResult> BossResultPresentation;

	/** Result Presentation 수명 동안 보존하는 완료 순간 남은 시간입니다 */
	float BossResultRemainingTimeSeconds = 0.0f;

	/** Result Presentation 수명 동안 보존하는 완료 순간 피격 횟수입니다 */
	int32 BossResultHitCount = 0;

	/** Controller가 인게임 메뉴의 Pause와 입력 수명을 소유 중인지 나타냅니다 */
	bool bIsInGameMenuOpen = false;

	/** 메뉴가 닫힐 때 외부 시스템이 만들었던 기존 Pause를 유지하기 위한 Snapshot입니다 */
	bool bWasGamePausedBeforeInGameMenu = false;
};
