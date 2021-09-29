// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Util/ProgressCancel.h"
#include "ModelingOperators.h"

#include "BaseOps/VoxelBaseOp.h"

#include "VoxelMorphologyMeshesOp.generated.h"


/** Morphology operation types */
UENUM()
enum class EMorphologyOperation : uint8
{
	/** Expand the shapes outward */
	Dilate = 0,

	/** Shrink the shapes inward */
	Contract = 1,

	/** Dilate and then contract, to delete small negative features (sharp inner corners, small holes) */
	Close = 2,

	/** Contract and then dilate, to delete small positive features (sharp outer corners, small isolated pieces) */
	Open = 3

};

class MODELINGOPERATORS_API FVoxelMorphologyMeshesOp : public FVoxelBaseOp
{
public:
	virtual ~FVoxelMorphologyMeshesOp() {}

	// inputs
	TArray<TSharedPtr<const FDynamicMesh3>> Meshes;
	TArray<FTransform> Transforms; // 1:1 with Meshes

	double Distance = 1.0;
	EMorphologyOperation Operation;

	bool bSolidifyInput = false;
	bool bRemoveInternalsAfterSolidify = false;
	double OffsetSolidifySurface = 0.0;

	void SetTransform(const FTransform& Transform);

	//
	// FDynamicMeshOperator implementation
	// 

	virtual void CalculateResult(FProgressCancel* Progress) override;

};


