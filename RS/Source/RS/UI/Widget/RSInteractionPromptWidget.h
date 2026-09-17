#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RSInteractionPromptWidget.generated.h"

class UTextBlock;

/**
 * 선택된 상호작용 대상 위에 표시할 문구를 보여 줍니다
 * 인스턴스는 대상이 아니라 탐색 Ability가 하나만 소유하며 선택이 바뀔 때 대상으로 옮겨 붙습니다
 */
UCLASS(Abstract, Blueprintable)
class RS_API URSInteractionPromptWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 대상이 제공한 문구를 표시합니다 */
	void SetPromptText(const FText& InPromptText);

protected:
	/** Blueprint가 같은 이름으로 배치하면 C++에서 직접 갱신할 문구 Text Block입니다 */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Interaction", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PromptTextBlock;
};
