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
	/** 저장 에셋에 저작한 두 Action Button의 클릭 이벤트를 연결합니다 */
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
	/** 현재 Level 재시작을 요청하는 Button입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "RS|Boss Result", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> Button_Restart;

	/** Main Menu Level 이동을 요청하는 Button입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "RS|Boss Result", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> Button_MainMenu;

	/** Widget의 Action 의도를 HUD에 전달하는 출력 이벤트입니다 */
	FRSBossResultActionRequested OnBossResultActionRequested;
};
