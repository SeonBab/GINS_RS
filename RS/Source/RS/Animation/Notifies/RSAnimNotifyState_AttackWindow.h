// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "RSAnimNotifyState_AttackWindow.generated.h"

/** Montage에서 실제 공격 판정이 진행되는 구간을 표시합니다 */
UCLASS(meta = (DisplayName = "Attack Window"))
class RS_API URSAnimNotifyState_AttackWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	URSAnimNotifyState_AttackWindow(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual FString GetNotifyName_Implementation() const override;
};
