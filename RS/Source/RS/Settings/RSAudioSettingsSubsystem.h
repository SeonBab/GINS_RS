// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "RSAudioVolumeSettings.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RSAudioSettingsSubsystem.generated.h"

class USoundMix;

/** 저장된 사용자 음량과 설정 화면의 임시 음량을 실제 오디오 출력에 적용합니다 */
UCLASS()
class RS_API URSAudioSettingsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** 저장값 로드와 World별 Sound Mix 적용을 준비합니다 */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** 임시 설정과 World Delegate를 정리합니다 */
	virtual void Deinitialize() override;

public:
	/** 현재 저장값을 Snapshot으로 보관하고 설정 편집을 시작합니다 */
	bool BeginAudioSettingsEdit(FRSAudioVolumeSettings& OutEditingSettings);

	/** 편집값을 저장하지 않고 현재 오디오 출력에 즉시 적용합니다 */
	bool PreviewAudioSettings(const FRSAudioVolumeSettings& InSettings);

	/** 편집값을 실제 설정으로 확정하고 사용자 설정 파일에 저장합니다 */
	bool ApplyAudioSettingsEdit();

	/** 편집 전 저장값으로 실제 출력을 복구하고 편집을 종료합니다 */
	bool CancelAudioSettingsEdit();

	/** 기본값을 편집값과 현재 출력에 임시 적용합니다 */
	bool PreviewDefaultAudioSettings(FRSAudioVolumeSettings& OutEditingSettings);

	/** 현재 실제 출력에 적용하도록 요청한 음량 설정을 반환합니다 */
	const FRSAudioVolumeSettings& GetCurrentAudioSettings() const { return CurrentAudioSettings; }

	/** 오디오 설정을 편집 중인지 반환합니다 */
	bool IsEditingAudioSettings() const { return EditTransaction.IsActive(); }

private:
	/** 새 Game World에 현재 음량을 다시 적용합니다 */
	void HandlePostWorldInitialization(UWorld* World, const UWorld::InitializationValues InitializationValues);

	/** 현재 음량을 지정한 World의 Sound Mix에 반영합니다 */
	bool ApplyCurrentAudioSettings(UWorld* World);

	/** 현재 GameInstance의 Game World를 반환합니다 */
	UWorld* GetAudioWorld() const;

	/** Engine이 관리하는 RS 사용자 설정을 반환합니다 */
	class URSGameUserSettings* GetRSGameUserSettings() const;

private:
	/** 저장값 Snapshot과 임시 적용값의 편집 상태입니다 */
	FRSAudioSettingsEditTransaction EditTransaction;

	/** 현재 World의 Sound Mix Override에 적용할 값입니다 */
	FRSAudioVolumeSettings CurrentAudioSettings;

	/** 새 Game World의 초기화를 관찰하는 Delegate Handle입니다 */
	FDelegateHandle PostWorldInitializationHandle;

	/** Sound Mix를 Push한 현재 Game World입니다 */
	TWeakObjectPtr<UWorld> AppliedWorld;

	/** 현재 World에 Push한 Runtime Sound Mix입니다 */
	TWeakObjectPtr<USoundMix> AppliedSoundMix;
};
