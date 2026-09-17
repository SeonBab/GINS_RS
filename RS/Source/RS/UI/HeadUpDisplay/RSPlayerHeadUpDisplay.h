// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "RSBossResultAction.h"
#include "RSPlayerHeadUpDisplay.generated.h"

class URSBossResultWidget;
class URSDialogueWidget;
class URSInGameMenuWidget;
class URSPrimaryLayout;
class URSScreenFadeWidget;
enum class ERSBossEncounterResult : uint8;
enum class ERSInGameMenuAction : uint8;

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

	/** 정적으로 배치된 Boss Result Widget을 확정 결과와 완료 Snapshot으로 표시합니다 */
	void ShowBossResultPresentation(ERSBossEncounterResult Result, float RemainingTimeSeconds, int32 HitCount);

	/** 정적으로 배치된 Boss Result Widget의 두 Action Button 상태를 함께 변경합니다 */
	void SetBossResultActionsEnabled(bool bEnabled);

	/** Menu Layer에 정적으로 배치된 인게임 메뉴를 표시하고 반환합니다 */
	URSInGameMenuWidget* ShowInGameMenu();

	/** 표시 중인 인게임 메뉴를 숨깁니다 */
	void HideInGameMenu();

	/** 인게임 메뉴의 세 Action Button 상태를 함께 변경합니다 */
	void SetInGameMenuActionsEnabled(bool bEnabled);

	/** Modal Layer에 생성한 대화 Widget에 본문을 설정하고 표시합니다 */
	URSDialogueWidget* ShowDialogue(const FText& DialogueText);

	/** 표시 중인 대화 Widget을 숨깁니다 */
	void HideDialogue();

	/** 화면을 어둡게 만들고 연출이 끝난 뒤 전달받은 동작을 실행합니다 */
	bool PlayScreenTransition(const FSimpleDelegate& OnFadedOut);

private:
	/** 설정된 클래스에서 루트 레이아웃을 생성하고 공유 ViewModel을 연결합니다 */
	void CreatePrimaryLayout();

	/** 루트 레이아웃에 선언된 수동 ViewModel 소스를 로컬 플레이어의 공유 인스턴스로 설정합니다 */
	void SetSharedViewModels();

	/** Menu Layer에 정적으로 배치된 Boss Result Widget을 반환합니다 */
	URSBossResultWidget* FindBossResultWidget() const;

	/** Transition Layer에 정적으로 배치된 화면 전환 Widget을 반환합니다 */
	URSScreenFadeWidget* FindScreenFadeWidget() const;

	/** Menu Layer에 정적으로 배치된 인게임 메뉴를 반환합니다 */
	URSInGameMenuWidget* FindInGameMenuWidget() const;

	/** 인게임 메뉴의 Action 이벤트를 연결하고 초기 표시 상태를 구성합니다 */
	void InitializeInGameMenu();

	/** 설정된 대화 Widget을 Modal Layer에 한 번 생성하고 닫기 이벤트를 연결합니다 */
	void InitializeDialogue();

	/** Boss Result Widget의 Action 의도를 Local PlayerController에 전달합니다 */
	void HandleBossResultActionRequested(ERSBossResultAction Action);

	/** 인게임 메뉴의 게임 계속 의도를 Local PlayerController에 전달합니다 */
	void HandleInGameMenuContinueRequested();

	/** 인게임 메뉴의 Level 전환 의도를 Local PlayerController에 전달합니다 */
	void HandleInGameMenuActionRequested(ERSInGameMenuAction Action);

	/** 대화 Widget의 닫기 의도를 Local PlayerController에 전달합니다 */
	void HandleDialogueCloseRequested();

private:
	/** 인게임 플레이어 화면에 사용할 루트 레이아웃 클래스입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|User Interface", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<URSPrimaryLayout> PrimaryLayoutClass;

	/** 현재 화면에 표시되는 루트 레이아웃입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "RS|User Interface", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URSPrimaryLayout> PrimaryLayout;

	/** Modal Layer에 생성할 튜토리얼 대화 Widget 클래스입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|User Interface", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<URSDialogueWidget> DialogueWidgetClass;

	/** HUD가 Modal Layer에 한 번 생성해 재사용하는 대화 Widget입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "RS|User Interface", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URSDialogueWidget> DialogueWidget;
};
