// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RSAudioVolumeSettings.generated.h"

/** 사용자 오디오 채널의 정규화된 음량 설정입니다 */
USTRUCT(BlueprintType)
struct RS_API FRSAudioVolumeSettings
{
	GENERATED_BODY()

public:
	/** 모든 오디오 채널에 공통으로 적용할 음량입니다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RS|Audio", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MasterVolume = 1.0f;

	/** 배경음 채널에 적용할 상대 음량입니다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RS|Audio", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BackgroundMusicVolume = 1.0f;

	/** 효과음과 UI 오디오 채널에 적용할 상대 음량입니다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RS|Audio", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SoundEffectsVolume = 1.0f;

	/** 모든 음량을 유효 범위로 제한한 복사본을 반환합니다 */
	FRSAudioVolumeSettings GetClamped() const;

	/** 세 채널이 지정한 설정과 허용 오차 안에서 같은지 반환합니다 */
	bool IsNearlyEqual(const FRSAudioVolumeSettings& Other, float Tolerance = KINDA_SMALL_NUMBER) const;
};
