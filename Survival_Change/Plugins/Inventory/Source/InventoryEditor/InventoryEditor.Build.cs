// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class InventoryEditor : ModuleRules
{
	public InventoryEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Inventory",
			"AssetRegistry",
			"GameplayTags",
			"UnrealEd",
			"AssetTools",
			"DataTableEditor",
			"Slate",
			"SlateCore",
			"InputCore",
			"PropertyEditor",
			"EditorStyle"
		});
	}
}
