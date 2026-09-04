// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "RSCheatManager.generated.h"

/**
 * 개발 중 상태를 직접 만들어 확인하기 위한 콘솔 명령을 모읍니다
 * UCheatManager는 Shipping 빌드에서 생성되지 않으므로 각 명령에 별도의 전처리 조건을 붙이지 않습니다
 */
UCLASS()
class RS_API URSCheatManager : public UCheatManager
{
	GENERATED_BODY()

public:
	/**
	 * 누운 상태 태그를 직접 부여하거나 제거합니다
	 * 넉다운 흐름 전체를 거치지 않고 상태에 따라 달라지는 표시와 판정을 확인할 때 사용합니다
	 * 어빌리티가 부여한 태그와 섞이므로 GA_Downed가 활성인 동안에는 사용하지 않습니다
	 */
	UFUNCTION(Exec)
	void RS_ToggleDownedTag();

private:
	/** 이 치트가 직접 부여한 상태인지 기록하여 어빌리티가 부여한 태그를 제거하지 않게 합니다 */
	bool bDownedTagApplied = false;
};
