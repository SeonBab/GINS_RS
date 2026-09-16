// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RSInGameMenuAction.generated.h"

/** 인게임 메뉴에서 게임 규칙 계층에 요청할 Level 전환 Action입니다 */
UENUM(BlueprintType)
enum class ERSInGameMenuAction : uint8
{
	/** 현재 Level을 처음부터 다시 시작합니다 */
	RestartLevel,

	/** Main Menu Level로 이동합니다 */
	ReturnToMainMenu
};
