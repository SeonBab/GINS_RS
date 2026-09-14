// Fill out your copyright notice in the Description page of Project Settings.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "RSAudioVolumeSettings.h"
#include "RSGameUserSettings.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSAudioSettingsTest, "RS.UI.AudioSettings", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSAudioSettingsTest::RunTest(const FString& Parameters)
{
	FRSAudioVolumeSettings InvalidSettings;
	InvalidSettings.MasterVolume = -0.25f;
	InvalidSettings.BackgroundMusicVolume = 0.4f;
	InvalidSettings.SoundEffectsVolume = 1.25f;

	const FRSAudioVolumeSettings ClampedSettings = InvalidSettings.GetClamped();
	TestEqual(TEXT("Master volume clamps to zero"), ClampedSettings.MasterVolume, 0.0f);
	TestEqual(TEXT("Background music volume remains unchanged"), ClampedSettings.BackgroundMusicVolume, 0.4f);
	TestEqual(TEXT("Sound effects volume clamps to one"), ClampedSettings.SoundEffectsVolume, 1.0f);

	FRSAudioSettingsEditTransaction EditTransaction;
	FRSAudioVolumeSettings SavedSettings;
	SavedSettings.MasterVolume = 0.8f;
	SavedSettings.BackgroundMusicVolume = 0.6f;
	SavedSettings.SoundEffectsVolume = 0.4f;

	TestTrue(TEXT("Editing begins from saved settings"), EditTransaction.Begin(SavedSettings));
	TestFalse(TEXT("Nested editing is rejected"), EditTransaction.Begin(FRSAudioVolumeSettings()));
	TestTrue(TEXT("Editing value begins at saved snapshot"), EditTransaction.GetEditingSettings().IsNearlyEqual(SavedSettings));

	FRSAudioVolumeSettings PreviewSettings;
	PreviewSettings.MasterVolume = 0.5f;
	PreviewSettings.BackgroundMusicVolume = 0.7f;
	PreviewSettings.SoundEffectsVolume = 0.9f;
	TestTrue(TEXT("Preview values update while editing"), EditTransaction.SetEditingSettings(PreviewSettings));
	TestTrue(TEXT("Preview values are stored separately"), EditTransaction.GetEditingSettings().IsNearlyEqual(PreviewSettings));
	TestTrue(TEXT("Saved snapshot remains unchanged during preview"), EditTransaction.GetSavedSettingsSnapshot().IsNearlyEqual(SavedSettings));
	TestTrue(TEXT("Cancel returns the saved snapshot"), EditTransaction.Cancel().IsNearlyEqual(SavedSettings));
	TestFalse(TEXT("Editing ends after cancel"), EditTransaction.IsActive());

	TestTrue(TEXT("Editing can begin again"), EditTransaction.Begin(SavedSettings));
	TestTrue(TEXT("Preview can be updated before commit"), EditTransaction.SetEditingSettings(PreviewSettings));
	TestTrue(TEXT("Commit returns the preview values"), EditTransaction.Commit().IsNearlyEqual(PreviewSettings));
	TestFalse(TEXT("Editing ends after commit"), EditTransaction.IsActive());
	TestTrue(TEXT("Committed values become the new snapshot"), EditTransaction.GetSavedSettingsSnapshot().IsNearlyEqual(PreviewSettings));

	URSGameUserSettings* GameUserSettings = NewObject<URSGameUserSettings>();
	TestNotNull(TEXT("Game user settings can be constructed"), GameUserSettings);
	if (GameUserSettings)
	{
		GameUserSettings->SetAudioVolumeSettings(InvalidSettings);
		TestTrue(TEXT("Game user settings clamp assigned values"), GameUserSettings->GetAudioVolumeSettings().IsNearlyEqual(ClampedSettings));

		GameUserSettings->SetToDefaults();
		TestTrue(TEXT("Game user settings restore all audio defaults"), GameUserSettings->GetAudioVolumeSettings().IsNearlyEqual(FRSAudioVolumeSettings()));
	}

	return true;
}

#endif
