// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

namespace UnrealBuildTool.Rules
{
	public class DataRegistryEditor : ModuleRules
	{
		public DataRegistryEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePaths.AddRange(
				new string[] {
					"DataRegistryEditor/Private",
				});

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"AssetTools",
					"GameplayTags",
					"DataRegistry",
					"GameplayTagsEditor",
					"AssetManagerEditor",
					"DataTableEditor",
					"InputCore",
					"PropertyEditor",
					"Slate",
					"SlateCore",
					"EditorStyle",
					"BlueprintGraph",
					"Kismet",
					"KismetCompiler",
					"GraphEditor",
					"UnrealEd",
					"WorkspaceMenuStructure",
					"ContentBrowser",
					"EditorWidgets",
					"ApplicationCore",
				}
			);
		}
	}
}
