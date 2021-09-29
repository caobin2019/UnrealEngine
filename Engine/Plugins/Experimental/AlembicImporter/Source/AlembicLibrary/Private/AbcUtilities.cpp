// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbcUtilities.h"

#include "AbcFile.h"
#include "AbcImportUtilities.h"
#include "GeometryCache.h"
#include "Materials/Material.h"

void FAbcUtilities::GetFrameMeshData(FAbcFile& AbcFile, int32 FrameIndex, FGeometryCacheMeshData& OutMeshData, int32 ConcurrencyIndex)
{
	AbcFile.ReadFrame(FrameIndex, EFrameReadFlags::ApplyMatrix, ConcurrencyIndex);

	FGeometryCacheMeshData MeshData;
	int32 PreviousNumVertices = 0;
	bool bConstantTopology = false;
	const bool bUseVelocitiesAsMotionVectors = true;

	AbcImporterUtilities::MergePolyMeshesToMeshData(FrameIndex, 0, AbcFile.GetSecondsPerFrame(), bUseVelocitiesAsMotionVectors,
		AbcFile.GetPolyMeshes(), AbcFile.GetUniqueFaceSetNames(), MeshData, PreviousNumVertices, bConstantTopology);

	OutMeshData = MoveTemp(MeshData);

	AbcFile.CleanupFrameData(ConcurrencyIndex);
}

void FAbcUtilities::SetupGeometryCacheMaterials(FAbcFile& AbcFile, UGeometryCache* GeometryCache, UObject* Package)
{
	GeometryCache->Materials.Reset();

	const EObjectFlags Flags = RF_Public | RF_Standalone | RF_Transactional;
	UMaterial* DefaultMaterial = UMaterial::GetDefaultMaterial(MD_Surface);

	for (const FString& FaceSetName : AbcFile.GetUniqueFaceSetNames())
	{
		UMaterialInterface* Material = AbcImporterUtilities::RetrieveMaterial(AbcFile, FaceSetName, Package, Flags);
		GeometryCache->Materials.Add((Material != nullptr) ? Material : DefaultMaterial);		

		if (Material != UMaterial::GetDefaultMaterial(MD_Surface))
		{
			Material->PostEditChange();
		}
	}
}
