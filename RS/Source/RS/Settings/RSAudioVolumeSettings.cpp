// Fill out your copyright notice in the Description page of Project Settings.

#include "RSAudioVolumeSettings.h"

FRSAudioVolumeSettings FRSAudioVolumeSettings::GetClamped() const
{
	FRSAudioVolumeSettings Result = *this;
	Result.MasterVolume = FMath::Clamp(Result.MasterVolume, 0.0f, 1.0f);
	Result.BackgroundMusicVolume = FMath::Clamp(Result.BackgroundMusicVolume, 0.0f, 1.0f);
	Result.SoundEffectsVolume = FMath::Clamp(Result.SoundEffectsVolume, 0.0f, 1.0f);

	return Result;
}

bool FRSAudioVolumeSettings::IsNearlyEqual(const FRSAudioVolumeSettings& Other, float Tolerance) const
{
	return FMath::IsNearlyEqual(MasterVolume, Other.MasterVolume, Tolerance)
		&& FMath::IsNearlyEqual(BackgroundMusicVolume, Other.BackgroundMusicVolume, Tolerance)
		&& FMath::IsNearlyEqual(SoundEffectsVolume, Other.SoundEffectsVolume, Tolerance);
}
