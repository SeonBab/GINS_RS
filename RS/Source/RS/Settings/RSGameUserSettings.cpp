// Fill out your copyright notice in the Description page of Project Settings.

#include "RSGameUserSettings.h"

#include "Engine/Engine.h"

void URSGameUserSettings::SetToDefaults()
{
	Super::SetToDefaults();

	AudioVolumeSettings = FRSAudioVolumeSettings();
}

void URSGameUserSettings::ValidateSettings()
{
	Super::ValidateSettings();

	AudioVolumeSettings = AudioVolumeSettings.GetClamped();
}

URSGameUserSettings* URSGameUserSettings::Get()
{
	return GEngine ? Cast<URSGameUserSettings>(GEngine->GetGameUserSettings()) : nullptr;
}

void URSGameUserSettings::SetAudioVolumeSettings(const FRSAudioVolumeSettings& InSettings)
{
	AudioVolumeSettings = InSettings.GetClamped();
}

