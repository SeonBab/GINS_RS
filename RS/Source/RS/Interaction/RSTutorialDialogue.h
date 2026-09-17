// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RSInteractable.h"
#include "RSTutorialDialogue.generated.h"

class UBoxComponent;
class USceneComponent;
class UStaticMeshComponent;

/**
 * 배치 인스턴스마다 외형과 본문을 바꿀 수 있는 튜토리얼 대화 대상입니다
 * 상호작용 탐지 형상과 표시 Mesh의 Collision을 분리해 외형 변경이 Focus 판정에 영향을 주지 않게 합니다
 */
UCLASS(Blueprintable)
class RS_API ARSTutorialDialogue : public AActor, public IRSInteractable
{
	GENERATED_BODY()

public:
	ARSTutorialDialogue();

public:
	virtual FText GetPromptText_Implementation() const override;
	virtual USceneComponent* GetPromptAnchor_Implementation() const override;
	virtual void Interact_Implementation(AActor* InteractInstigator) override;

private:
	/** 외형과 상호작용 형상의 공통 배치 기준입니다 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Dialogue", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneRoot;

	/** 배치 인스턴스마다 Mesh와 Transform을 바꾸는 시각 표현입니다 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Dialogue", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> DisplayMesh;

	/** 외형과 독립적으로 조정하는 상호작용 탐지 형상입니다 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Dialogue", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> InteractionBounds;

	/** 프롬프트를 붙일 위치이며 배치 인스턴스마다 조정합니다 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Dialogue", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> PromptAnchor;

	/** 상호작용 프롬프트에 표시할 짧은 문구입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Dialogue", meta = (AllowPrivateAccess = "true"))
	FText PromptText;

	/** 이 대상과 상호작용했을 때 표시할 기획자 편집 본문입니다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RS|Dialogue", meta = (MultiLine = "true", AllowPrivateAccess = "true"))
	FText DialogueText;
};
