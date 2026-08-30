// Fill out your copyright notice in the Description page of Project Settings.


#include "RSGameModeBase.h"

#include "RSPlayerState.h"
#include "RSPlayerCharacter.h"

ARSGameModeBase::ARSGameModeBase()
{
	PlayerStateClass = ARSPlayerState::StaticClass();
	DefaultPawnClass = ARSPlayerCharacter::StaticClass();
}
