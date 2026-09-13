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

/** 설정 화면에서 저장값 Snapshot과 임시 적용값을 분리해 관리합니다 */
class RS_API FRSAudioSettingsEditTransaction
{
public:
	/** 저장값을 Snapshot과 편집값으로 복사해 새 편집을 시작합니다 */
	bool Begin(const FRSAudioVolumeSettings& SavedSettings);

	/** 편집 중인 값을 교체합니다 */
	bool SetEditingSettings(const FRSAudioVolumeSettings& InSettings);

	/** 편집값을 확정하고 편집을 종료합니다 */
	FRSAudioVolumeSettings Commit();

	/** 저장값 Snapshot을 반환하고 편집을 종료합니다 */
	FRSAudioVolumeSettings Cancel();

	/** 현재 편집 중인지 반환합니다 */
	bool IsActive() const { return bIsActive; }

	/** 현재 임시 적용할 편집값을 반환합니다 */
	const FRSAudioVolumeSettings& GetEditingSettings() const { return EditingSettings; }

	/** 취소할 때 복구할 저장값 Snapshot을 반환합니다 */
	const FRSAudioVolumeSettings& GetSavedSettingsSnapshot() const { return SavedSettingsSnapshot; }

private:
	/** 취소 시 복구할 편집 시작 시점의 저장값입니다 */
	FRSAudioVolumeSettings SavedSettingsSnapshot;

	/** 현재 Runtime 출력에 임시 적용할 값입니다 */
	FRSAudioVolumeSettings EditingSettings;

	/** 중첩 편집과 편집 외 요청을 차단하는 상태입니다 */
	bool bIsActive = false;
};
