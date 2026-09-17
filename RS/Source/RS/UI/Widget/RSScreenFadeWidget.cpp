// Fill out your copyright notice in the Description page of Project Settings.

#include "RSScreenFadeWidget.h"

#include "Animation/WidgetAnimation.h"

void URSScreenFadeWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	FWidgetAnimationDynamicEvent FadeOutFinished;
	FadeOutFinished.BindDynamic(this, &ThisClass::HandleFadeOutFinished);
	BindToAnimationFinished(FadeOut, FadeOutFinished);
}

void URSScreenFadeWidget::PlayFadeIn()
{
	// Fade Out이 진행 중이면 곧 화면이 바뀌므로 다시 밝히지 않습니다
	if (bIsFadeOutPending || !FadeIn)
	{
		return;
	}

	PlayAnimation(FadeIn);
}

bool URSScreenFadeWidget::PlayFadeOutThen(const FSimpleDelegate& OnFadedOut)
{
	if (bIsFadeOutPending || !FadeOut || !OnFadedOut.IsBound())
	{
		return false;
	}

	StopAllAnimations();
	PendingAction = OnFadedOut;
	bIsFadeOutPending = true;
	PlayAnimation(FadeOut);

	return true;
}

float URSScreenFadeWidget::GetFadeOutDuration() const
{
	return FadeOut ? FadeOut->GetEndTime() : 0.0f;
}

void URSScreenFadeWidget::HandleFadeOutFinished()
{
	if (!bIsFadeOutPending)
	{
		return;
	}

	// 실행 중 재진입을 막기 위해 보관한 동작을 먼저 떼어 냅니다
	FSimpleDelegate ActionToExecute = MoveTemp(PendingAction);
	PendingAction.Unbind();
	ActionToExecute.ExecuteIfBound();
}
