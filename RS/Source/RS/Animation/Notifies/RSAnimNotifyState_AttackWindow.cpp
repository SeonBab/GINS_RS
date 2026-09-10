// Fill out your copyright notice in the Description page of Project Settings.

#include "RSAnimNotifyState_AttackWindow.h"

URSAnimNotifyState_AttackWindow::URSAnimNotifyState_AttackWindow(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 동시에 재생되는 Montage 인스턴스의 구간 표식이 하나의 Notify 수명으로 병합되지 않게 합니다
	NotifyStateBehaviorFlags = static_cast<uint8>(EAnimNotifyStateBehaviorFlags::NoMergeOnConcurrentPlay);
}

FString URSAnimNotifyState_AttackWindow::GetNotifyName_Implementation() const
{
	return TEXT("Attack Window");
}
