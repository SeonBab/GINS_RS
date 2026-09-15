// Fill out your copyright notice in the Description page of Project Settings.

#include "RSMusicPlaybackSubsystem.h"

#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

void URSMusicPlaybackSubsystem::Deinitialize()
{
	DestroyMusicComponents();
	Super::Deinitialize();
}

bool URSMusicPlaybackSubsystem::PlayMusic(USoundBase* Music, float CrossfadeDuration)
{
	if (!Music || !GetWorld())
	{
		return false;
	}

	if (CurrentMusic == Music && ActiveMusicComp && ActiveMusicComp->IsPlaying())
	{
		return true;
	}

	const float ClampedCrossfadeDuration = FMath::Max(0.0f, CrossfadeDuration);
	FadeOutActiveMusic(ClampedCrossfadeDuration);

	UAudioComponent* NewMusicComp = CreateMusicComponent(Music);
	if (!NewMusicComp)
	{
		CurrentMusic = nullptr;
		return false;
	}

	ActiveMusicComp = NewMusicComp;
	CurrentMusic = Music;
	NewMusicComp->FadeIn(ClampedCrossfadeDuration, 1.0f);
	return true;
}

void URSMusicPlaybackSubsystem::StopMusic(float FadeOutDuration)
{
	CurrentMusic = nullptr;
	FadeOutActiveMusic(FMath::Max(0.0f, FadeOutDuration));
}

UAudioComponent* URSMusicPlaybackSubsystem::CreateMusicComponent(USoundBase* Music)
{
	UAudioComponent* MusicComp = UGameplayStatics::CreateSound2D(GetWorld(), Music, 1.0f, 1.0f, 0.0f, nullptr, false, false);
	if (!MusicComp)
	{
		return nullptr;
	}

	MusicComp->OnAudioFinishedNative.AddUObject(this, &ThisClass::HandleAudioFinished);
	return MusicComp;
}

void URSMusicPlaybackSubsystem::HandleAudioFinished(UAudioComponent* FinishedAudioComponent)
{
	if (!FinishedAudioComponent)
	{
		return;
	}

	if (ActiveMusicComp == FinishedAudioComponent)
	{
		ActiveMusicComp = nullptr;
		CurrentMusic = nullptr;
	}

	FadingOutMusicComps.Remove(FinishedAudioComponent);
	FinishedAudioComponent->OnAudioFinishedNative.RemoveAll(this);
	FinishedAudioComponent->DestroyComponent();
}

void URSMusicPlaybackSubsystem::FadeOutActiveMusic(float FadeOutDuration)
{
	UAudioComponent* MusicComp = ActiveMusicComp;
	ActiveMusicComp = nullptr;
	if (!MusicComp)
	{
		return;
	}

	FadingOutMusicComps.Add(MusicComp);
	if (FadeOutDuration > 0.0f)
	{
		MusicComp->FadeOut(FadeOutDuration, 0.0f);
		return;
	}

	MusicComp->Stop();
}

void URSMusicPlaybackSubsystem::DestroyMusicComponents()
{
	TArray<UAudioComponent*> MusicComps;
	if (ActiveMusicComp)
	{
		MusicComps.Add(ActiveMusicComp);
	}

	for (UAudioComponent* FadingOutMusicComp : FadingOutMusicComps)
	{
		if (FadingOutMusicComp)
		{
			MusicComps.AddUnique(FadingOutMusicComp);
		}
	}

	ActiveMusicComp = nullptr;
	FadingOutMusicComps.Reset();
	CurrentMusic = nullptr;

	for (UAudioComponent* MusicComp : MusicComps)
	{
		MusicComp->OnAudioFinishedNative.RemoveAll(this);
		MusicComp->Stop();
		MusicComp->DestroyComponent();
	}
}
