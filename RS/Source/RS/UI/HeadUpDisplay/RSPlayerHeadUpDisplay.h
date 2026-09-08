// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "RSBossResultAction.h"
#include "RSPlayerHeadUpDisplay.generated.h"

class URSBossResultWidget;
class URSPrimaryLayout;
enum class ERSBossEncounterResult : uint8;

/** 인게임 플레이어 화면의 루트 위젯과 공유 ViewModel 연결을 관리합니다 */
UCLASS()
class RS_API ARSPlayerHeadUpDisplay : public AHUD
{
	GENERATED_BODY()

protected:
	/** 로컬 플레이어의 루트 레이아웃을 생성하고 화면에 표시합니다 */
	virtual void BeginPlay() override;

	/** HUD 종료 시 생성한 루트 레이아웃을 화면에서 제거합니다 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	/** 생성된 플레이어 화면의 루트 레이아웃을 반환합니다 */
	UFUNCTION(BlueprintPure, Category = "RS|User Interface")
	URSPrimaryLayout* GetPrimaryLayout() const;

	/** 정적으로 배치된 Boss Result Widget을 확정 결과와 함께 표시합니다 */
	void ShowBossResultPresentation(ERSBossEncounterResult Result);

	/** 정적으로 배치된 Boss Result Widget의 두 Action Button 상태를 함께 변경합니다 */
	void SetBossResultActionsEnabled(bool bEnabled);

private:
	/** 설정된 클래스에서 루트 레이아웃을 생성하고 공유 ViewModel을 연결합니다 */
	void CreatePrimaryLayout();

	/** 루트 레이아웃에 선언된 수동 ViewModel 소스를 로컬 플레이어의 공유 인스턴스로 설정합니다 */
	void SetSharedViewModels();

	/** Menu Layer에 정적으로 배치된 Boss Result Widget을 반환합니다 */
	URSBossResultWidget* FindBossResultWidget() const;

	/** Boss Result Widget의 Action 의도를 Local PlayerController에 전달합니다 */
	void HandleBossResultActionRequested(ERSBossResultAction Action);

private:
	/** 인게임 플레이어 화면에 사용할 루트 레이아웃 클래스입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|User Interface", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<URSPrimaryLayout> PrimaryLayoutClass;

	/** 현재 화면에 표시되는 루트 레이아웃입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "RS|User Interface", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URSPrimaryLayout> PrimaryLayout;
};
