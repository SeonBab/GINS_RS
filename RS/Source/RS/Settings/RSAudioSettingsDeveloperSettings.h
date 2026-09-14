// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RSAudioSettingsDeveloperSettings.generated.h"

class USoundClass;
class USoundMix;

/** 프로젝트 전체에서 공유하는 오디오 설정 에셋 참조입니다 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "RS Audio Settings"))
class RS_API URSAudioSettingsDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Runtime 음량 Override에 사용할 Sound Mix를 반환합니다 */
	const TSoftObjectPtr<USoundMix>& GetSoundMix() const { return SoundMix; }

	/** 모든 채널의 부모 Sound Class를 반환합니다 */
	const TSoftObjectPtr<USoundClass>& GetMasterSoundClass() const { return MasterSoundClass; }

	/** 배경음 채널 Sound Class를 반환합니다 */
	const TSoftObjectPtr<USoundClass>& GetBackgroundMusicSoundClass() const { return BackgroundMusicSoundClass; }

	/** 효과음과 UI 오디오 채널 Sound Class를 반환합니다 */
	const TSoftObjectPtr<USoundClass>& GetSoundEffectsSoundClass() const { return SoundEffectsSoundClass; }

private:
	/** Runtime 음량 Override를 보관할 Sound Mix입니다 */
	UPROPERTY(Config, EditAnywhere, Category = "RS|Audio", meta = (AllowedClasses = "/Script/Engine.SoundMix"))
	TSoftObjectPtr<USoundMix> SoundMix;

	/** 배경음과 효과음 채널에 공통 배율을 적용할 부모 Sound Class입니다 */
	UPROPERTY(Config, EditAnywhere, Category = "RS|Audio", meta = (AllowedClasses = "/Script/Engine.SoundClass"))
	TSoftObjectPtr<USoundClass> MasterSoundClass;

	/** 배경음에 상대 배율을 적용할 Sound Class입니다 */
	UPROPERTY(Config, EditAnywhere, Category = "RS|Audio", meta = (AllowedClasses = "/Script/Engine.SoundClass"))
	TSoftObjectPtr<USoundClass> BackgroundMusicSoundClass;

	/** 효과음과 UI 오디오에 상대 배율을 적용할 Sound Class입니다 */
	UPROPERTY(Config, EditAnywhere, Category = "RS|Audio", meta = (AllowedClasses = "/Script/Engine.SoundClass"))
	TSoftObjectPtr<USoundClass> SoundEffectsSoundClass;
};

