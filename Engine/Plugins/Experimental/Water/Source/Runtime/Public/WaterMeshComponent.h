// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/Class.h"
#include "Engine/EngineTypes.h"
#include "Components/MeshComponent.h"
#include "WaterQuadTree.h"
#include "WaterMeshComponent.generated.h"

bool IsWaterMeshEnabled(bool bIsRenderThread);

/**
 * Water Mesh Component responsible for generating and rendering a continuous water mesh on top of all the existing water body actors in the world
 * The component contains a quadtree which defines where there are water tiles. A function for traversing the quadtree and outputing a list of instance data for each tile to be rendered from a point of view is included
 */
UCLASS(ClassGroup = (Rendering, Water), hidecategories = (Object, Activation, "Components|Activation"), editinlinenew)
class WATER_API UWaterMeshComponent : public UMeshComponent
{
	GENERATED_BODY()

public:
	UWaterMeshComponent();

	//~ Begin UObject Interface
	virtual void PostLoad() override;
	virtual void PostInitProperties() override;
	//~ End UObject Interface

	//~ Begin UMeshComponent Interface
	virtual int32 GetNumMaterials() const override { return 0; }
	//~ End UMeshComponent Interface

	//~ Begin UPrimitiveComponent Interface
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials = false) const override;
	virtual bool ShouldRenderSelected() const override;
	//~ End UPrimitiveComponent Interface

	void Update();

	/** Use this instead of GetMaterialRelevance, since this one will go over all materials from all tiles */
	FMaterialRelevance GetWaterMaterialRelevance(ERHIFeatureLevel::Type InFeatureLevel) const;

	const FWaterQuadTree& GetWaterQuadTree() const { return WaterQuadTree; }

	const TSet<UMaterialInterface*>& GetUsedMaterialsSet() const { return UsedMaterials; }

	FWaterTileInstanceData GetFarDistanceInstanceData() const { return FarDistanceWaterInstanceData; }

	void MarkWaterMeshGridDirty() { bNeedsRebuild = true; }

	int32 GetTessellationFactor() const { return FMath::Clamp(TessellationFactor + TessFactorBiasScalability, 1, 12); }

	float GetLODScale() const { return LODScale + LODScaleBiasScalability; }

	/** At above what density level a tile is allowed to force collapse even if not all leaf nodes in the subtree are present.
	 *	Collapsing will not occus if any child node in the subtree has different materials.
	 *	Setting this to -1 means no collapsing is allowed and the water mesh will always keep it's silhouette at any distance.
	 *	Setting this to 0 will allow every level to collapse
	 *	Setting this to something higher than the LODCount will have no effect
	 */
	UPROPERTY(EditAnywhere, Category = Mesh, meta = (ClampMin = "-1"))
	int32 ForceCollapseDensityLevel = -1;

	/** World size of the water tiles at LOD0. Multiply this with the ExtentInTiles to get the world extents of the system */
	UPROPERTY(EditAnywhere, Category = Mesh, meta = (ClampMin = "100"))
	float TileSize = 2400.0f;

	/** The extent of the system in number of tiles. Maximum number of tiles for this system will be ExtentInTiles.X*2*ExtentInTiles.Y*2 */
	UPROPERTY(EditAnywhere, Category = Mesh, meta = (ClampMin = "1"))
	FIntPoint ExtentInTiles = FIntPoint(64, 64);

	UPROPERTY(EditAnywhere, Category = "Mesh|FarDistance")
	UMaterialInterface* FarDistanceMaterial = nullptr;

	UPROPERTY(EditAnywhere, Category = "Mesh|FarDistance", meta = (ClampMin = "0"))
	float FarDistanceMeshExtent = 0.0f;

	UFUNCTION(BlueprintPure, Category = Mesh)
	bool IsEnabled() const { return bIsEnabled; }

	// HACK [jonathan.bard] (start) : This is to make sure that the RTWorldLocation / RTWorldSizeVector MPC params can be serialized and set at runtime on the Water MPC.
	//  It used to be handled by AWaterBrushManager, which is not available on client builds. 
	//  This should be handled 1) not through a MPC and 2) not through a landscape-specific tool-only thing such as AWaterBrushManager : 
	void SetLandscapeInfo(const FVector& InRTWorldLocation, const FVector& InRTWorldSizeVector);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Texture)
	FVector RTWorldLocation;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Texture)
	FVector RTWorldSizeVector;
	// HACK [jonathan.bard] (end)

private:
	//~ Begin USceneComponent Interface
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	//~ Begin USceneComponent Interface

	/** Based on all water bodies in the scene, rebuild the water mesh */
	void RebuildWaterMesh(float InTileSize, const FIntPoint& InExtentInTiles);

	// HACK [jonathan.bard] : see SetLandscapeInfo
	void UpdateWaterMPC();

	/** Tiles containing water, stored in a quad tree */
	FWaterQuadTree WaterQuadTree;

	/** Unique list of materials used by this component */
	UPROPERTY(Transient, NonPIEDuplicateTransient, TextExportTransient)
	TSet<UMaterialInterface*> UsedMaterials;

	/** Dirty flag which will make sure the water mesh is updated properly */
	bool bNeedsRebuild = true;

	/** If the system is enabled */
	bool bIsEnabled = false;

	/** Cached CVarWaterMeshLODCountBias to detect changes in scalability */
	int32 LODCountBiasScalability = 0;
	
	/** Cached CVarWaterMeshTessFactorBias to detect changes in scalability */
	int32 TessFactorBiasScalability = 0;

	/** Cached CVarWaterMeshLODScaleBias to detect changes in scalability */
	float LODScaleBiasScalability = 0.0f;

	/** Instance data for the far distance mesh */
	FWaterTileInstanceData FarDistanceWaterInstanceData;

	/** Highest tessellation factor of a water tile. Max number of verts on the side of a tile will be (2^TessellationFactor)+1)  */
	UPROPERTY(EditAnywhere, Category = Mesh, meta = (ClampMin = "1", ClampMax = "12"))
	int32 TessellationFactor = 6;

	/** World scale of the concentric LODs */
	UPROPERTY(EditAnywhere, Category = Mesh, meta = (ClampMin = "0.5"))
	float LODScale = 1.0f;

#if WITH_EDITOR
	//~ Begin USceneComponent Interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ Begin USceneComponent Interface
#endif
};