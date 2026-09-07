// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h"
#include "RSAnimNotify_GameplayEvent.generated.h"

/**
 * 애니메이션 타임라인의 한 시점에 소유 Actor의 ASC로 Gameplay Event를 보냅니다
 * 이 Notify는 시점만 알리고 판정과 상태 변경은 이벤트를 받는 Ability가 담당합니다
 */
UCLASS(meta = (DisplayName = "Gameplay Event"))
class RS_API URSAnimNotify_GameplayEvent : public UAnimNotify
{
	GENERATED_BODY()

public:
	URSAnimNotify_GameplayEvent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

public:
	/** 이 Notify가 보낼 Gameplay Event 태그를 반환합니다 */
	FGameplayTag GetEventTag() const { return EventTag; }

protected:
	/** 이 시점에 소유 Actor로 보낼 Gameplay Event 태그입니다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RS|Gameplay Event", meta = (Categories = "GameplayEvent"))
	FGameplayTag EventTag;
};
