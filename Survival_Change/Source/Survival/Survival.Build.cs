// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Survival : ModuleRules
{
	public Survival(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"Inventory"
        });

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"Survival",
			"Survival/Variant_Platforming",
			"Survival/Variant_Platforming/Animation",
			"Survival/Variant_Combat",
			"Survival/Variant_Combat/AI",
			"Survival/Variant_Combat/Animation",
			"Survival/Variant_Combat/Gameplay",
			"Survival/Variant_Combat/Interfaces",
			"Survival/Variant_Combat/UI",
			"Survival/Variant_SideScrolling",
			"Survival/Variant_SideScrolling/AI",
			"Survival/Variant_SideScrolling/Gameplay",
			"Survival/Variant_SideScrolling/Interfaces",
			"Survival/Variant_SideScrolling/UI",
        });

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
