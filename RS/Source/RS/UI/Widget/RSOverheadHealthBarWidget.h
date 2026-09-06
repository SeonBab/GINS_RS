// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RSViewModelWidget.h"
#include "RSOverheadHealthBarWidget.generated.h"

/**
 * 캐릭터 머리 위에 배치되어 플레이어의 체력 비율을 표시하는 위젯입니다
 * 위젯 컴포넌트가 생성하는 위젯은 HUD의 주입 대상이 아니므로 저장소에서 직접 ViewModel을 얻습니다
 */
UCLASS(Abstract)
class RS_API URSOverheadHealthBarWidget : public URSViewModelWidget
{
	GENERATED_BODY()

protected:
	/** 위젯 계층이 준비되면 플레이어 상태 ViewModel을 연결합니다 */
	virtual void NativeOnInitialized() override;

private:
	/** 로컬 플레이어 저장소에서 플레이어 상태 ViewModel을 얻어 위젯에 설정합니다 */
	void InitializePlayerStatusViewModel();
};
