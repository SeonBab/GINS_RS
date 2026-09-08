#pragma once

#include "CoreMinimal.h"
#include "RSBaseCharacter.h"
#include "AbilitySystemInterface.h"
#include "RSAbilitySet.h"
#include "RSBossCharacter.generated.h"

class ARSBossEncounter;
class URSAbilitySystemComponent;
class URSAttackTelegraphComponent;
class URSBossPhaseComponent;
class URSHealthComponent;
class URSHealthSet;

/** 보스 전용 설정과 Controller 연결의 기반이 되는 캐릭터입니다 */
UCLASS()
class RS_API ARSBossCharacter : public ARSBaseCharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	/** 보스가 배치되거나 생성될 때 BossController가 자동으로 빙의하도록 기본값을 구성합니다 */
	ARSBossCharacter();

protected:
	/** HealthComponent의 사망 시작 이벤트에 보스 전용 처리를 연결합니다 */
	virtual void PostInitializeComponents() override;

	/** 보스가 직접 소유한 ASC를 초기화하고 기본 AbilitySet을 부여합니다 */
	virtual void BeginPlay() override;

#pragma region GAS

public:
	/** 보스 Character가 직접 소유한 ASC를 반환합니다 */
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	/** 보스 Character가 직접 소유한 RS ASC를 반환합니다 */
	URSAbilitySystemComponent* GetRSAbilitySystemComponent() const { return AbilitySystemComp; }

	/** 보스 Character가 직접 소유한 HealthSet을 반환합니다 */
	const URSHealthSet* GetHealthSet() const { return HealthSet; }

	/** 보스의 체력 변경과 사망 시작을 전달하는 HealthComponent를 반환합니다 */
	URSHealthComponent* GetHealthComponent() const { return HealthComp; }

	/** 보스 공격의 바닥 예고를 그리는 표시 컴포넌트를 반환합니다 */
	URSAttackTelegraphComponent* GetAttackTelegraphComponent() const { return AttackTelegraphComp; }

	/** 보스 전투의 진행 상태와 패턴 선택 정책을 소유하는 컴포넌트를 반환합니다 */
	URSBossPhaseComponent* GetBossPhaseComponent() const { return BossPhaseComp; }

private:
	/** ASC의 Owner와 Avatar를 보스 Character로 초기화하고 기본 AbilitySet을 한 번 부여합니다 */
	void InitializeAbilitySystem();

	/** 사망이 시작되면 실행 중인 Ability와 보스 AI의 행동을 중지합니다 */
	UFUNCTION()
	void HandleDeathStarted(URSHealthComponent* InHealthComponent);

protected:
	/** 이 보스가 초기화될 때 기본으로 부여할 AbilitySet입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Abilities")
	TObjectPtr<URSAbilitySet> DefaultAbilitySet;

private:
	/** 보스의 Ability와 Gameplay Effect 상태를 소유하는 ASC입니다 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Abilities", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URSAbilitySystemComponent> AbilitySystemComp;

	/** 보스의 체력 Attribute를 ASC와 같은 생명주기로 유지합니다 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Health", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URSHealthSet> HealthSet;

	/** HealthSet의 체력 변경을 외부 시스템에 전달하고 사망 상태 태그를 관리합니다 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Health", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URSHealthComponent> HealthComp;

	/** 공격 범위를 바닥에 미리 그리는 표시 컴포넌트입니다 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Telegraph", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URSAttackTelegraphComponent> AttackTelegraphComp;

	/** 패턴 사이클과 패턴 후보 목록을 소유하는 컴포넌트입니다 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Boss", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URSBossPhaseComponent> BossPhaseComp;

	/** DefaultAbilitySet이 부여한 Ability를 정확히 식별하기 위한 핸들입니다 */
	UPROPERTY(Transient)
	FRSAbilitySet_GrantedHandles GrantedAbilityHandles;

	/** 기본 AbilitySet의 중복 부여를 방지합니다 */
	bool bDefaultAbilitiesGranted = false;

#pragma endregion

public:
	/** 보스 Blueprint에 설정된 이름을 반환합니다 */
	const FText& GetBossName() const { return BossName; }

	/** 보스 체력의 표시 레이어 수가 설정 경로와 무관하게 최소 1이 되도록 반환합니다 */
	int32 GetHealthLayerCount() const { return FMath::Max(HealthLayerCount, 1); }

	/** 이 캐릭터의 전투 참가자와 생명주기를 관리할 Encounter를 설정합니다 */
	void SetBossEncounter(ARSBossEncounter* InBossEncounter);

	/** 보스는 생존시킨 채 진행 중인 공격과 이동을 정리하고 Encounter 전투 상태에서 이탈합니다 */
	void EndEncounterCombat();

	/** 이 캐릭터에 연결된 Encounter를 반환합니다 */
	ARSBossEncounter* GetBossEncounter() const { return BossEncounter; }

private:
	/** 보스의 이름을 클래스 기본값으로 설정합니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Boss", meta = (AllowPrivateAccess = "true"))
	FText BossName;

	/** 실제 체력은 나누어 저장하지 않고 사용자 인터페이스에서 균등 분할해 표시합니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Boss", meta = (AllowPrivateAccess = "true", ClampMin = "1", UIMin = "1"))
	int32 HealthLayerCount = 1;

	/** 보스전 상태와 참가자 목록을 소유하는 Encounter입니다 */
	UPROPERTY(Transient)
	TObjectPtr<ARSBossEncounter> BossEncounter;
};
