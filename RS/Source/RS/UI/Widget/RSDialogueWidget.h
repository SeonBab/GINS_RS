// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RSDialogueWidget.generated.h"

class APlayerController;
class UTextBlock;

DECLARE_MULTICAST_DELEGATE(FRSDialogueCloseRequested)

/** 고정된 화자 이름과 전달받은 튜토리얼 본문을 표시하고 닫기 의도를 전달합니다 */
UCLASS(Abstract, Blueprintable)
class RS_API URSDialogueWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	URSDialogueWidget(const FObjectInitializer& ObjectInitializer);

protected:
	/** 정적으로 배치된 Widget이 처음에는 보이지 않도록 초기 상태를 구성합니다 */
	virtual void NativeOnInitialized() override;

	/** G와 Escape 입력을 닫기 요청으로 소비합니다 */
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	/** 보이는 대화창의 좌클릭과 우클릭을 닫기 요청으로 소비합니다 */
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

public:
	/** 전달받은 본문을 설정하고 Widget을 표시합니다 */
	bool ShowDialogue(const FText& InDialogueText);

	/** Widget을 숨깁니다 */
	void HideDialogue();

	/** G와 Escape를 Widget이 먼저 처리할 수 있도록 Keyboard Focus를 요청합니다 */
	void RequestInitialFocus(APlayerController* PlayerController);

	/** 닫기 입력 의도를 소유자에게 전달하는 이벤트를 반환합니다 */
	FRSDialogueCloseRequested& GetCloseRequested() { return OnCloseRequested; }

private:
	/** 모든 닫기 입력을 같은 이벤트로 전달합니다 */
	void RequestClose();

private:
	/** Blueprint가 같은 이름으로 배치한 튜토리얼 본문 Text Block입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "RS|Dialogue", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> TextBlock_DialogueContent;

	/** G, Escape 또는 대화창 클릭으로 발생한 닫기 의도입니다 */
	FRSDialogueCloseRequested OnCloseRequested;
};
