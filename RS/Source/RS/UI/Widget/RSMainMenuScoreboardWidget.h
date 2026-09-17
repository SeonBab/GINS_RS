#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RSMainMenuScoreboardWidget.generated.h"

class APlayerController;
class UButton;
class URSScoreboardPanelWidget;

DECLARE_MULTICAST_DELEGATE(FRSMainMenuScoreboardClosed)

/** 공용 스코어보드 Panel과 Main Menu 복귀 Action을 조합합니다 */
UCLASS()
class RS_API URSMainMenuScoreboardWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

public:
	/** 공용 Panel을 열고 현재 Cache를 표시합니다 */
	bool OpenScoreboard();

	/** 뒤로 Button에 Keyboard Focus를 요청합니다 */
	void RequestInitialFocus(APlayerController* PlayerController);

	/** Main Menu 복귀 요청 이벤트를 반환합니다 */
	FRSMainMenuScoreboardClosed& GetMainMenuScoreboardClosed() { return OnMainMenuScoreboardClosed; }

private:
	UFUNCTION()
	void HandleCloseButtonClicked();

private:
	UPROPERTY(Transient, BlueprintReadOnly, Category = "RS|Main Menu", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<URSScoreboardPanelWidget> Widget_Scoreboard;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "RS|Main Menu", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> Button_Close;

	FRSMainMenuScoreboardClosed OnMainMenuScoreboardClosed;
};
