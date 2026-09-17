// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RSScreenFadeWidget.generated.h"

class UWidgetAnimation;

/**
 * 화면 전체를 덮는 Fade 연출을 재생하고 Fade Out이 끝난 시점을 알립니다
 * 목적지 Level, Pause, 입력과 오디오는 알지 않으며 전달받은 동작을 보관했다가 실행하기만 합니다
 */
UCLASS(Abstract)
class RS_API URSScreenFadeWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	/** Fade Out 완료 통지를 연결합니다 */
	virtual void NativeOnInitialized() override;

public:
	/** 검정 화면에서 밝아지는 연출을 재생합니다 */
	void PlayFadeIn();

	/** 화면을 어둡게 만든 뒤 전달받은 동작을 실행합니다. 이미 진행 중이면 요청을 무시합니다 */
	bool PlayFadeOutThen(const FSimpleDelegate& OnFadedOut);

	/** 현재 Fade Out 애니메이션의 길이를 반환합니다 */
	float GetFadeOutDuration() const;

	/** Fade Out이 진행 중인지 반환합니다 */
	bool IsFadeOutPending() const { return bIsFadeOutPending; }

private:
	/** Fade Out이 끝나면 보관한 동작을 한 번만 실행합니다 */
	UFUNCTION()
	void HandleFadeOutFinished();

private:
	/** 검정 화면에서 밝아지는 애니메이션입니다 */
	UPROPERTY(Transient, meta = (BindWidgetAnim, AllowPrivateAccess = "true"))
	TObjectPtr<UWidgetAnimation> FadeIn;

	/** 화면을 검정으로 덮는 애니메이션입니다 */
	UPROPERTY(Transient, meta = (BindWidgetAnim, AllowPrivateAccess = "true"))
	TObjectPtr<UWidgetAnimation> FadeOut;

	/** Fade Out이 끝난 뒤 실행할 동작입니다 */
	FSimpleDelegate PendingAction;

	/** 전환 요청이 중복으로 접수되지 않도록 진행 상태를 기록합니다 */
	bool bIsFadeOutPending = false;
};
