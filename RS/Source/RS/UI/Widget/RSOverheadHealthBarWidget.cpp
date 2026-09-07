// Fill out your copyright notice in the Description page of Project Settings.


#include "RSOverheadHealthBarWidget.h"

#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "RSLocalPlayerViewModelSubsystem.h"
#include "RSPlayerStatusViewModel.h"

void URSOverheadHealthBarWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	InitializePlayerStatusViewModel();
}

void URSOverheadHealthBarWidget::InitializePlayerStatusViewModel()
{
	// 위젯 컴포넌트는 에디터 월드에서도 위젯을 생성하며 그곳에는 연결할 로컬 플레이어가 없습니다
	const UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}

	// 루트 레이아웃의 Manual 소스만 HUD가 주입하므로, 위젯 컴포넌트가 생성한 이 위젯은 저장소에서 직접 ViewModel을 얻습니다
	const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	if (!LocalPlayer)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s has no owning LocalPlayer"), *GetNameSafe(this));
		return;
	}

	URSLocalPlayerViewModelSubsystem* ViewModelSubsystem = LocalPlayer->GetSubsystem<URSLocalPlayerViewModelSubsystem>();
	URSPlayerStatusViewModel* PlayerStatusViewModel = ViewModelSubsystem ? ViewModelSubsystem->GetOrCreateViewModel<URSPlayerStatusViewModel>() : nullptr;
	if (!PlayerStatusViewModel)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s has no URSPlayerStatusViewModel"), *GetNameSafe(this));
		return;
	}

	// 위젯에 선언한 ViewModel 소스와 맞지 않으면 표시가 갱신되지 않으므로 실패를 남깁니다
	if (!SetViewModel(PlayerStatusViewModel))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s failed to set URSPlayerStatusViewModel"), *GetNameSafe(this));
	}
}
