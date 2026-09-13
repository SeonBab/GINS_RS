// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "RSAudioVolumeSettings.h"
#include "RSGameUserSettings.generated.h"

/** RS의 사용자별 영속 설정을 관리합니다 */
UCLASS(Config = GameUserSettings)
class RS_API URSGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	/** 저장된 값이 없을 때 RS 기본 설정으로 초기화합니다 */
	virtual void SetToDefaults() override;

	/** 로드하거나 외부에서 지정한 값을 유효 범위로 제한합니다 */
	virtual void ValidateSettings() override;

public:
	/** Engine이 관리하는 현재 RS 사용자 설정 객체를 반환합니다 */
	static URSGameUserSettings* Get();

	/** 저장 대상으로 보관 중인 오디오 설정을 반환합니다 */
	const FRSAudioVolumeSettings& GetAudioVolumeSettings() const { return AudioVolumeSettings; }

	/** 저장할 오디오 설정을 유효 범위로 제한해 보관합니다 */
	void SetAudioVolumeSettings(const FRSAudioVolumeSettings& InSettings);

private:
	/** 사용자별로 저장하는 Master·배경음·효과음 값입니다 */
	UPROPERTY(Config)
	FRSAudioVolumeSettings AudioVolumeSettings;
};
