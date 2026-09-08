// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RSBossResultAction.h"
#include "RSViewModelWidget.h"
#include "RSBossResultWidget.generated.h"

class UButton;

DECLARE_MULTICAST_DELEGATE_OneParam(FRSBossResultActionRequested, ERSBossResultAction)

/** 확정된 보스전 결과의 제목과 안내 문구를 표시하는 공통 Widget 기반입니다 */
UCLASS()
class RS_API URSBossResultWidget : public URSViewModelWidget
{
	GENERATED_BODY()

protected:
	/** 저장된 Result 레이아웃 아래에 두 Action Button을 한 번 구성합니다 */
	virtual void NativeOnInitialized() override;

public:
	/** Result Action 요청 이벤트를 반환합니다 */
	FRSBossResultActionRequested& GetBossResultActionRequested() { return OnBossResultActionRequested; }

	/** 두 Result Action Button의 입력 가능 상태를 함께 변경합니다 */
	void SetActionsEnabled(bool bEnabled);

private:
	/** 현재 Level 재시작 Action을 요청합니다 */
	UFUNCTION()
	void HandleRestartButtonClicked();

	/** Main Menu Level 이동 Action을 요청합니다 */
	UFUNCTION()
	void HandleMainMenuButtonClicked();

private:
	/** 런타임에 구성한 현재 Level 재시작 Button입니다 */
	UPROPERTY(Transient)
	TObjectPtr<UButton> RestartButton;

	/** 런타임에 구성한 Main Menu 이동 Button입니다 */
	UPROPERTY(Transient)
	TObjectPtr<UButton> MainMenuButton;

	/** Widget의 Action 의도를 HUD에 전달하는 출력 이벤트입니다 */
	FRSBossResultActionRequested OnBossResultActionRequested;
};
