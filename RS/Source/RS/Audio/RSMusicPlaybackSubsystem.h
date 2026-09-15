// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RSMusicPlaybackSubsystem.generated.h"

class UAudioComponent;
class USoundBase;

/** 현재 World의 2D 음악 재생과 곡 전환 수명을 관리합니다 */
UCLASS()
class RS_API URSMusicPlaybackSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** World가 제거되기 전에 생성한 Audio Component를 정리합니다 */
	virtual void Deinitialize() override;

	/** 같은 곡은 유지하고 다른 곡은 지정한 시간 동안 Crossfade합니다 */
	UFUNCTION(BlueprintCallable, Category = "RS|Music")
	bool PlayMusic(USoundBase* Music, float CrossfadeDuration = 1.0f);

	/** 현재 곡을 지정한 시간 동안 Fade Out한 뒤 정지합니다 */
	UFUNCTION(BlueprintCallable, Category = "RS|Music")
	void StopMusic(float FadeOutDuration = 1.0f);

	/** 현재 재생 대상으로 관리하는 음악을 반환합니다 */
	UFUNCTION(BlueprintPure, Category = "RS|Music")
	USoundBase* GetCurrentMusic() const { return CurrentMusic; }

private:
	/** 지정한 음악을 재생할 World 수명의 Audio Component를 생성합니다 */
	UAudioComponent* CreateMusicComponent(USoundBase* Music);

	/** 재생을 마친 Component의 참조를 해제하고 제거합니다 */
	void HandleAudioFinished(UAudioComponent* FinishedAudioComponent);

	/** 현재 Component를 Fade Out 목록으로 옮깁니다 */
	void FadeOutActiveMusic(float FadeOutDuration);

	/** Subsystem이 생성한 모든 Component를 즉시 정리합니다 */
	void DestroyMusicComponents();

private:
	/** 현재 음악을 재생하는 Component입니다 */
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> ActiveMusicComp;

	/** Crossfade 중 종료를 기다리는 이전 음악 Component입니다 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UAudioComponent>> FadingOutMusicComps;

	/** 동일 곡 중복 요청을 구분하는 현재 음악입니다 */
	UPROPERTY(Transient)
	TObjectPtr<USoundBase> CurrentMusic;
};
