// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class DataprepGeometryOperations : ModuleRules
	{
		public DataprepGeometryOperations(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"DataprepCore",
					"DataprepLibraries",
					"DynamicMesh",
					"EditorScriptingUtilities",
					"Engine",
					"GeometricObjects",
					"MeshConversion",
					"MeshDescription",
					"MeshMergeUtilities",
					"MeshModelingTools",
					"MeshUtilitiesCommon",
					"MeshProcessingLibrary",
					"ModelingOperators",
					"ModelingOperatorsEditorOnly",
					"MeshReductionInterface",
					"StaticMeshDescription",
					"StaticMeshEditorExtension",
					"UnrealEd",
				}
			);
		}
	}
}
