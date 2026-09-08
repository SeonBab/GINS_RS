// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RSBossResultAction.generated.h"

/** 보스전 Result UI에서 요청할 수 있는 상위 Game Rule 동작입니다 */
UENUM(BlueprintType)
enum class ERSBossResultAction : uint8
{
	RestartLevel,
	ReturnToMainMenu
};
