// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RSViewModelWidget.h"
#include "RSPrimaryLayout.generated.h"

class UPanelWidget;

/** 루트 사용자 인터페이스에서 서로 겹칠 수 있는 위젯 계층을 구분합니다 */
UENUM(BlueprintType)
enum class ERSWidgetLayer : uint8
{
	/** 항상 표시되는 게임플레이 사용자 인터페이스 계층입니다 */
	Gameplay,

	/** 게임플레이 화면 위에 표시되는 메뉴 계층입니다 */
	Menu,

	/** 다른 입력을 차단하는 화면을 표시하는 계층입니다 */
	Modal,

	/** 알림을 가장 앞에 표시하는 계층입니다 */
	Notification
};

/** 플레이어 화면의 루트 위젯과 계층별 위젯 생명주기를 관리합니다 */
UCLASS(Abstract)
class RS_API URSPrimaryLayout : public URSViewModelWidget
{
	GENERATED_BODY()

public:
	/** 이 레이아웃을 소유자로 사용하는 위젯을 생성하고 지정한 계층에 추가합니다 */
	UFUNCTION(BlueprintCallable, Category = "RS|User Interface")
	UUserWidget* CreateWidgetInLayer(ERSWidgetLayer Layer, TSubclassOf<UUserWidget> WidgetClass);

	/** 이미 생성된 위젯을 지정한 계층에 추가합니다 */
	UFUNCTION(BlueprintCallable, Category = "RS|User Interface")
	bool AddWidgetToLayer(ERSWidgetLayer Layer, UWidget* Widget);

	/** 이 레이아웃의 계층에 추가된 위젯을 제거합니다 */
	UFUNCTION(BlueprintCallable, Category = "RS|User Interface")
	bool RemoveWidgetFromLayer(UWidget* Widget);

	/** 지정한 사용자 인터페이스 계층의 패널을 반환합니다 */
	UFUNCTION(BlueprintPure, Category = "RS|User Interface")
	UPanelWidget* GetLayer(ERSWidgetLayer Layer) const;

private:
	/** 항상 표시되는 게임플레이 사용자 인터페이스 계층입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "RS|User Interface", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UPanelWidget> GameplayLayer;

	/** 게임플레이 화면 위에 표시되는 메뉴 계층입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "RS|User Interface", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UPanelWidget> MenuLayer;

	/** 다른 입력을 차단하는 화면을 표시하는 계층입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "RS|User Interface", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UPanelWidget> ModalLayer;

	/** 알림을 가장 앞에 표시하는 계층입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "RS|User Interface", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UPanelWidget> NotificationLayer;
};
