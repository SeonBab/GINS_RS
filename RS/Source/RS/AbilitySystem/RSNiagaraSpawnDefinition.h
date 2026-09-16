// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RSNiagaraSpawnDefinition.generated.h"

class UNiagaraSystem;

/** Niagara를 월드 위치 또는 소켓 기준으로 생성하는 방식입니다 */
UENUM(BlueprintType)
enum class ERSNiagaraSpawnMode : uint8
{
	WorldTransform,
	SocketSnapshot,
	AttachedToSocket
};

/** Niagara 에셋과 생성 기준을 함께 보관하는 공용 정의입니다 */
USTRUCT(BlueprintType)
struct FRSNiagaraSpawnDefinition
{
	GENERATED_BODY()

	/** 정의 자체가 생성 가능한 조합인지 검사합니다 */
	bool IsDataValid(FString* OutValidationError = nullptr) const;

	/** 생성할 Niagara System입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Niagara")
	TObjectPtr<UNiagaraSystem> NiagaraSystem;

	/** 월드 Transform과 소켓 기준 생성 중 하나를 선택합니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Niagara")
	ERSNiagaraSpawnMode SpawnMode = ERSNiagaraSpawnMode::WorldTransform;

	/** 소켓 기준 생성에서 사용할 소켓 이름입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Niagara", meta = (EditCondition = "SpawnMode != ERSNiagaraSpawnMode::WorldTransform", EditConditionHides))
	FName SocketName;

	/** 소켓 기준 생성에서 소켓의 회전을 사용할지 정하며, 끄면 회전 없이 생성합니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Niagara", meta = (EditCondition = "SpawnMode != ERSNiagaraSpawnMode::WorldTransform", EditConditionHides))
	bool bUseSocketRotation = true;

	/** 소켓 기준 생성에서 소켓의 스케일을 사용할지 정하며, 끄면 스케일 1로 생성합니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Niagara", meta = (EditCondition = "SpawnMode != ERSNiagaraSpawnMode::WorldTransform", EditConditionHides))
	bool bUseSocketScale = true;
};
