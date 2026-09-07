// Copyright Epic Games, Inc. All Rights Reserved.

#include "RS.h"
#include "Modules/ModuleManager.h"
#include "AbilitySystem/Combat/RSCombatFunctionLibrary.h"

/**
 * RS 게임 모듈입니다
 * 콘솔 명령처럼 엔진에 등록해 두는 것은 전역 객체가 아니라 이 모듈의 시작과 끝에서 관리합니다
 * 핫 리로드는 이전 모듈을 종료한 뒤 새 DLL을 올리므로 이렇게 해야 이전 등록이 남아 종료 시 크래시로 이어지지 않습니다
 */
class FRSModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
#if !UE_BUILD_SHIPPING
		RSCombatDebug::RegisterConsoleCommands();
#endif
	}

	virtual void ShutdownModule() override
	{
#if !UE_BUILD_SHIPPING
		RSCombatDebug::UnregisterConsoleCommands();
#endif
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE( FRSModule, RS, "RS" );
