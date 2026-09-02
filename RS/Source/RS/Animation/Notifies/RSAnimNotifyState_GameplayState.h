// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "GameplayTagContainer.h"
#include "RSAnimNotifyState_GameplayState.generated.h"

/** 애니메이션 타임라인의 한 구간에 수명이 같은 게임플레이 상태 태그를 적용합니다 */
UCLASS(meta = (DisplayName = "Gameplay State"))
class RS_API URSAnimNotifyState_GameplayState : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	URSAnimNotifyState_GameplayState(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

protected:
	/** 이 Notify State의 시작과 종료 시각을 함께 사용하는 하나 이상의 실제 State 태그입니다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RS|Gameplay State", meta = (Categories = "State"))
	FGameplayTagContainer StateTags;
};
