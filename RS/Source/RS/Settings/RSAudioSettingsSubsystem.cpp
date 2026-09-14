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

	PostWorldInitializationHandle = FWorldDelegates::OnPostWorldInitialization.AddUObject(this, &ThisClass::HandlePostWorldInitialization);

	ApplyCurrentAudioSettings(GetAudioWorld());
}

void URSAudioSettingsSubsystem::Deinitialize()
{
	if (EditTransaction.IsActive())
	{
		CancelAudioSettingsEdit();
	}

	FWorldDelegates::OnPostWorldInitialization.Remove(PostWorldInitializationHandle);
	PostWorldInitializationHandle.Reset();

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

bool URSAudioSettingsSubsystem::BeginAudioSettingsEdit(FRSAudioVolumeSettings& OutEditingSettings)
{
	URSGameUserSettings* GameUserSettings = GetRSGameUserSettings();
	if (!GameUserSettings || !EditTransaction.Begin(GameUserSettings->GetAudioVolumeSettings()))
	{
		return false;
	}

	OutEditingSettings = EditTransaction.GetEditingSettings();
	CurrentAudioSettings = OutEditingSettings;
	ApplyCurrentAudioSettings(GetAudioWorld());

	return true;
}

bool URSAudioSettingsSubsystem::PreviewAudioSettings(const FRSAudioVolumeSettings& InSettings)
{
	if (!EditTransaction.SetEditingSettings(InSettings))
	{
		return false;
	}

	CurrentAudioSettings = EditTransaction.GetEditingSettings();
	ApplyCurrentAudioSettings(GetAudioWorld());

	return true;
}

bool URSAudioSettingsSubsystem::ApplyAudioSettingsEdit()
{
	URSGameUserSettings* GameUserSettings = GetRSGameUserSettings();
	if (!GameUserSettings || !EditTransaction.IsActive())
	{
		return false;
	}

	CurrentAudioSettings = EditTransaction.Commit();
	GameUserSettings->SetAudioVolumeSettings(CurrentAudioSettings);
	GameUserSettings->SaveSettings();
	ApplyCurrentAudioSettings(GetAudioWorld());

	return true;
}

bool URSAudioSettingsSubsystem::CancelAudioSettingsEdit()
{
	if (!EditTransaction.IsActive())
	{
		return false;
	}

	CurrentAudioSettings = EditTransaction.Cancel();
	ApplyCurrentAudioSettings(GetAudioWorld());

	return true;
}

bool URSAudioSettingsSubsystem::PreviewDefaultAudioSettings(FRSAudioVolumeSettings& OutEditingSettings)
{
	if (!EditTransaction.SetEditingSettings(FRSAudioVolumeSettings()))
	{
		return false;
	}

	OutEditingSettings = EditTransaction.GetEditingSettings();
	CurrentAudioSettings = OutEditingSettings;
	ApplyCurrentAudioSettings(GetAudioWorld());

	return true;
}

void URSAudioSettingsSubsystem::HandlePostWorldInitialization(UWorld* World, const UWorld::InitializationValues)
{
	if (!World || !World->IsGameWorld() || World->GetGameInstance() != GetGameInstance())
	{
		return;
	}

	ApplyCurrentAudioSettings(World);
}

bool URSAudioSettingsSubsystem::ApplyCurrentAudioSettings(UWorld* World)
{
	if (!World || !World->IsGameWorld())
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

	if (AppliedWorld.Get() != World || AppliedSoundMix.Get() != SoundMix)
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
