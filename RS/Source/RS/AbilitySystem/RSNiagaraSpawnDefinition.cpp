// Fill out your copyright notice in the Description page of Project Settings.

#include "RSNiagaraSpawnDefinition.h"

bool FRSNiagaraSpawnDefinition::IsDataValid(FString* OutValidationError) const
{
	auto SetError = [OutValidationError](const TCHAR* Message)
	{
		if (OutValidationError)
		{
			*OutValidationError = Message;
		}
	};

	if (!NiagaraSystem)
	{
		SetError(TEXT("NiagaraSystem is required."));

		return false;
	}

	if (SpawnMode != ERSNiagaraSpawnMode::WorldTransform && SocketName.IsNone())
	{
		SetError(TEXT("SocketName is required for a socket spawn mode."));

		return false;
	}

	return true;
}
