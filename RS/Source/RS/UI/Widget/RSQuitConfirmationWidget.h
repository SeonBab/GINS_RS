// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RSQuitConfirmationWidget.generated.h"

class APlayerController;
class UButton;

DECLARE_MULTICAST_DELEGATE(FRSQuitConfirmationAction)

/** Application 종료를 확정하거나 취소하는 확인 Widget입니다 */
UCLASS()
class RS_API URSQuitConfirmationWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	/** 확인과 취소 Button 이벤트를 연결합니다 */
	virtual void NativeOnInitialized() override;

	/** Escape 입력을 취소로 처리합니다 */
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

public:
	/** 종료 확정 이벤트를 반환합니다 */
	FRSQuitConfirmationAction& GetQuitConfirmed() { return OnQuitConfirmed; }

	/** 종료 취소 이벤트를 반환합니다 */
	FRSQuitConfirmationAction& GetQuitCancelled() { return OnQuitCancelled; }

	/** 실수로 종료하지 않도록 취소 Button에 Keyboard Focus를 요청합니다 */
	void RequestInitialFocus(APlayerController* PlayerController);

private:
	UFUNCTION()
	void HandleConfirmButtonClicked();

	UFUNCTION()
	void HandleCancelButtonClicked();

private:
	/** Application 종료를 확정하는 Button입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "RS|Main Menu", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> Button_Confirm;

	/** 종료를 취소하고 Main Menu로 돌아가는 Button입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "RS|Main Menu", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> Button_Cancel;

	/** 확인된 종료 의도를 PlayerController에 전달합니다 */
	FRSQuitConfirmationAction OnQuitConfirmed;

	/** 취소 의도를 PlayerController에 전달합니다 */
	FRSQuitConfirmationAction OnQuitCancelled;
};
