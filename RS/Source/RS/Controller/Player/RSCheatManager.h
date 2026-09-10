// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "RSCheatManager.generated.h"

class ARSBossEncounter;

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
	 * 누운 상태와 빠른 기상 가용 태그를 직접 부여하거나 제거합니다
	 * 넉다운 흐름 전체를 거치지 않고 상태에 따라 달라지는 표시와 판정을 확인할 때 사용합니다
	 * 어빌리티가 부여한 태그와 섞이므로 GA_Downed가 활성인 동안에는 사용하지 않습니다
	 */
	UFUNCTION(Exec)
	void RS_ToggleDownedTag();

	/**
	 * 진행 중인 보스전을 완료 상태로 만듭니다
	 * 보스 사망도 완료를 알리지만 보스를 실제로 처치하지 않고 완료 이후의 화면과 상태를 확인할 때 사용합니다
	 */
	UFUNCTION(Exec)
	void RS_CompleteBossEncounter();

	/**
	 * 진행 중인 보스전의 제한 시간을 즉시 만료시킵니다
	 * 제한 시간 전체를 기다리지 않고 만료 이후의 표시와 이벤트를 확인할 때 사용합니다
	 */
	UFUNCTION(Exec)
	void RS_ExpireBossTimeLimit();

	/**
	 * 플레이어를 가해자로 보스에게 피해를 적용합니다
	 * 실제 대미지 GameplayEffect 경로를 그대로 사용하므로 표시 계층까지 같은 흐름으로 확인할 수 있습니다
	 * RS의 데미지는 논리적으로 정수 단위이므로 인자도 정수로 받습니다
	 */
	UFUNCTION(Exec)
	void RS_DamageBoss(int32 Amount);

private:
	/**
	 * 명령을 적용할 보스 Encounter를 찾으며 없으면 nullptr입니다
	 * 진행 중인 전투를 우선하여 아직 시작하지 않은 Encounter를 대상으로 삼지 않습니다
	 */
	ARSBossEncounter* FindTargetBossEncounter() const;

private:
	/** 이 치트가 직접 부여한 상태인지 기록하여 어빌리티가 부여한 태그를 제거하지 않게 합니다 */
	bool bDownedTagApplied = false;
};
