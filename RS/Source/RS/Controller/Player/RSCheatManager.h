// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "RSCheatManager.generated.h"

class ARSBossEncounter;
class URSAbilitySystemComponent;

/**
 * 개발 중 상태를 직접 만들어 확인하기 위한 콘솔 명령을 모읍니다
 * UCheatManager는 Shipping 빌드에서 생성되지 않으므로 각 명령에 별도의 전처리 조건을 붙이지 않습니다
 */
UCLASS()
class RS_API URSCheatManager : public UCheatManager
{
	GENERATED_BODY()

public:
	/** CheatManager가 제거될 때 무적 명령으로 추가한 피해 면역 태그를 제거합니다 */
	virtual void BeginDestroy() override;

	/** 일반 공격과 같은 피해 GameplayEffect를 플레이어에게 적용해 체력 감소와 사망을 테스트합니다 */
	void DamagePlayer(int32 Amount);

	/** 살아 있는 플레이어에게 회복 GameplayEffect를 적용합니다 */
	void HealPlayer(int32 Amount);

	/** 무적 상태에서도 플레이어의 Health를 0으로 설정해 HealthComponent의 사망 처리를 실행합니다 */
	void KillPlayer();

	/**
	 * bEnabled가 true이면 플레이어에게 피해 면역 태그를 추가하고, false이면 이 명령이 추가한 태그만 제거합니다
	 * 다른 Ability가 부여한 피해 면역은 유지합니다
	 */
	void SetPlayerInvulnerable(bool bEnabled);

	/** 플레이어와 보스의 체력, 태그, 활성 Ability 및 Encounter 상태를 출력합니다 */
	void DumpState() const;

	/**
	 * 이름이 비어 있으면 현재 페이즈에서 선택 가능한 패턴 목록을 출력합니다
	 * 이름을 지정하면 실행 중인 패턴을 중단하지 않고, 같은 종류(Basic/Special)를 다음에 선택할 때 지정한 패턴을 한 번 선택합니다
	 */
	void RunBossPattern(const FString& PatternName);

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
	/** 이 CheatManager의 PlayerController에 연결된 플레이어 ASC를 반환하며, 유효한 캐릭터가 없으면 경고를 출력합니다 */
	URSAbilitySystemComponent* FindPlayerAbilitySystemComponent() const;

	/** 살아 있는 플레이어에게 Amount만큼 피해 또는 회복을 적용하며, 0 이하의 값은 거부합니다 */
	void ApplyPlayerHealthEffect(int32 Amount, bool bHealing);

	/** 무적 명령으로 추가한 피해 면역 태그만 제거해 다른 Ability의 면역을 유지합니다 */
	void ClearPlayerInvulnerability();

	/**
	 * 명령을 적용할 보스 Encounter를 찾으며 없으면 nullptr입니다
	 * 진행 중인 전투를 우선하여 아직 시작하지 않은 Encounter를 대상으로 삼지 않습니다
	 */
	ARSBossEncounter* FindTargetBossEncounter() const;

private:
	/** 플레이어가 교체되어도 무적 명령으로 태그를 추가했던 ASC를 찾아 해당 태그를 제거할 수 있도록 기억합니다 */
	TWeakObjectPtr<URSAbilitySystemComponent> InvulnerableAbilitySystemComp;

	/** 이 치트가 직접 부여한 상태인지 기록하여 어빌리티가 부여한 태그를 제거하지 않게 합니다 */
	bool bDownedTagApplied = false;
};
