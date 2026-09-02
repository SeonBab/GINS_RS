// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class RS : ModuleRules
{
	public RS(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(new string[] {
			Path.Combine(ModuleDirectory, "AbilitySystem"),
			Path.Combine(ModuleDirectory, "AbilitySystem", "Abilities"),
			Path.Combine(ModuleDirectory, "AbilitySystem", "Attributes"),
			Path.Combine(ModuleDirectory, "AbilitySystem", "Components"),
			Path.Combine(ModuleDirectory, "AnimInstance"),
			Path.Combine(ModuleDirectory, "Animation"),
			Path.Combine(ModuleDirectory, "Animation", "Notifies"),
			Path.Combine(ModuleDirectory, "Camera"),
			Path.Combine(ModuleDirectory, "Camera", "Boss"),
			Path.Combine(ModuleDirectory, "Camera", "Player"),
			Path.Combine(ModuleDirectory, "Character"),
			Path.Combine(ModuleDirectory, "Character", "Boss"),
			Path.Combine(ModuleDirectory, "Character", "Player"),
			Path.Combine(ModuleDirectory, "Controller"),
			Path.Combine(ModuleDirectory, "Controller", "Boss"),
			Path.Combine(ModuleDirectory, "Controller", "Player"),
			Path.Combine(ModuleDirectory, "Encounter"),
			Path.Combine(ModuleDirectory, "Encounter", "Boss"),
			Path.Combine(ModuleDirectory, "GameMode"),
			Path.Combine(ModuleDirectory, "GameplayTags"),
			Path.Combine(ModuleDirectory, "Input"),
			Path.Combine(ModuleDirectory, "PlayerState"),
			Path.Combine(ModuleDirectory, "UI"),
			Path.Combine(ModuleDirectory, "UI", "HeadUpDisplay"),
			Path.Combine(ModuleDirectory, "UI", "Layout"),
			Path.Combine(ModuleDirectory, "UI", "ViewModel"),
			Path.Combine(ModuleDirectory, "UI", "Widget"),
		});

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "AIModule", "InputCore", "EnhancedInput", "GameplayAbilities", "GameplayTags", "GameplayTasks", "UMG", "FieldNotification", "ModelViewViewModel" });

		PrivateDependencyModuleNames.AddRange(new string[] {  });

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
