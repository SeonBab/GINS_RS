// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "RSPlayerHeadUpDisplay.generated.h"

class URSPrimaryLayout;

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

private:
	/** 설정된 클래스에서 루트 레이아웃을 생성하고 공유 ViewModel을 연결합니다 */
	void CreatePrimaryLayout();

	/** 루트 레이아웃에 선언된 수동 ViewModel 소스를 로컬 플레이어의 공유 인스턴스로 설정합니다 */
	void SetSharedViewModels();

private:
	/** 인게임 플레이어 화면에 사용할 루트 레이아웃 클래스입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|User Interface", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<URSPrimaryLayout> PrimaryLayoutClass;

	/** 현재 화면에 표시되는 루트 레이아웃입니다 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "RS|User Interface", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URSPrimaryLayout> PrimaryLayout;
};
