// Fill out your copyright notice in the Description page of Project Settings.

#include "RSAudioSettingsSubsystem.h"

#include "Kismet/GameplayStatics.h"
#include "RSAudioSettingsDeveloperSettings.h"
#include "RSGameUserSettings.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"

void URSAudioSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (URSGameUserSettings* GameUserSettings = GetRSGameUserSettings())
	{
		GameUserSettings->LoadSettings(false);
		GameUserSettings->ValidateSettings();
		CurrentAudioSettings = GameUserSettings->GetAudioVolumeSettings();
	}

	WorldInitializedActorsHandle = FWorldDelegates::OnWorldInitializedActors.AddUObject(this, &ThisClass::HandleWorldInitializedActors);
}

void URSAudioSettingsSubsystem::Deinitialize()
{
	FWorldDelegates::OnWorldInitializedActors.Remove(WorldInitializedActorsHandle);
	WorldInitializedActorsHandle.Reset();

	if (UWorld* World = AppliedWorld.Get())
	{
		if (USoundMix* SoundMix = AppliedSoundMix.Get())
		{
			UGameplayStatics::PopSoundMixModifier(World, SoundMix);
		}
	}

	AppliedWorld.Reset();
	AppliedSoundMix.Reset();

	Super::Deinitialize();
}

void URSAudioSettingsSubsystem::SetAudioSettings(const FRSAudioVolumeSettings& InSettings)
{
	CurrentAudioSettings = InSettings.GetClamped();
	ApplyCurrentAudioSettings(GetAudioWorld());
}

bool URSAudioSettingsSubsystem::SaveAudioSettings()
{
	URSGameUserSettings* GameUserSettings = GetRSGameUserSettings();
	if (!GameUserSettings)
	{
		return false;
	}

	GameUserSettings->SetAudioVolumeSettings(CurrentAudioSettings);
	GameUserSettings->SaveSettings();

	return true;
}

void URSAudioSettingsSubsystem::HandleWorldInitializedActors(const FActorsInitializedParams& InitializationParams)
{
	UWorld* World = InitializationParams.World;
	if (!World || !World->IsGameWorld() || World->GetGameInstance() != GetGameInstance())
	{
		return;
	}

	World->OnWorldBeginPlay.AddUObject(this, &ThisClass::HandleWorldBeginPlay, World);
}

void URSAudioSettingsSubsystem::HandleWorldBeginPlay(UWorld* World)
{
	ApplyCurrentAudioSettings(World);
}

bool URSAudioSettingsSubsystem::ApplyCurrentAudioSettings(UWorld* World)
{
	if (!World || !World->IsGameWorld() || !World->GetAudioDevice())
	{
		return false;
	}

	const URSAudioSettingsDeveloperSettings* AudioSettings = GetDefault<URSAudioSettingsDeveloperSettings>();
	USoundMix* SoundMix = AudioSettings->GetSoundMix().LoadSynchronous();
	USoundClass* MasterSoundClass = AudioSettings->GetMasterSoundClass().LoadSynchronous();
	USoundClass* BackgroundMusicSoundClass = AudioSettings->GetBackgroundMusicSoundClass().LoadSynchronous();
	USoundClass* SoundEffectsSoundClass = AudioSettings->GetSoundEffectsSoundClass().LoadSynchronous();
	if (!SoundMix || !MasterSoundClass || !BackgroundMusicSoundClass || !SoundEffectsSoundClass)
	{
		return false;
	}

	const bool bNeedsSoundMixPush = AppliedWorld.Get() != World || AppliedSoundMix.Get() != SoundMix;
	if (bNeedsSoundMixPush)
	{
		if (UWorld* PreviousWorld = AppliedWorld.Get())
		{
			if (USoundMix* PreviousSoundMix = AppliedSoundMix.Get())
			{
				UGameplayStatics::PopSoundMixModifier(PreviousWorld, PreviousSoundMix);
			}
		}

		UGameplayStatics::PushSoundMixModifier(World, SoundMix);
		AppliedWorld = World;
		AppliedSoundMix = SoundMix;
	}

	UGameplayStatics::SetSoundMixClassOverride(World, SoundMix, MasterSoundClass, CurrentAudioSettings.MasterVolume, 1.0f, 0.0f, true);
	UGameplayStatics::SetSoundMixClassOverride(World, SoundMix, BackgroundMusicSoundClass, CurrentAudioSettings.BackgroundMusicVolume, 1.0f, 0.0f, false);
	UGameplayStatics::SetSoundMixClassOverride(World, SoundMix, SoundEffectsSoundClass, CurrentAudioSettings.SoundEffectsVolume, 1.0f, 0.0f, false);

	return true;
}

UWorld* URSAudioSettingsSubsystem::GetAudioWorld() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetWorld() : nullptr;
}

URSGameUserSettings* URSAudioSettingsSubsystem::GetRSGameUserSettings() const
{
	return URSGameUserSettings::Get();
}
