// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VectorTypes.h"
#include "FrameTypes.h"
#include "InteractiveTool.h"
#include "MeshBrushOpBase.generated.h"

class FDynamicMesh3;

enum class ESculptBrushOpTargetType : uint8
{
	SculptMesh,
	TargetMesh,
	ActivePlane
};


UENUM()
enum class EPlaneBrushSideMode : uint8
{
	BothSides = 0,
	PushDown = 1,
	PullTowards = 2
};


struct MESHMODELINGTOOLS_API FSculptBrushStamp
{
	FFrame3d WorldFrame;
	FFrame3d LocalFrame;
	double Radius;
	double Falloff;
	double Power;
	double Direction;
	double Depth;
	double DeltaTime;

	FFrame3d PrevWorldFrame;
	FFrame3d PrevLocalFrame;

	FDateTime TimeStamp;

	// only initialized if current op requires it
	FFrame3d RegionPlane;

	FSculptBrushStamp()
	{
		TimeStamp = FDateTime::Now();
	}
};


struct MESHMODELINGTOOLS_API FSculptBrushOptions
{
	//bool bPreserveUVFlow = false;

	FFrame3d ConstantReferencePlane;
};


class MESHMODELINGTOOLS_API FMeshSculptFallofFunc
{
public:
	TUniqueFunction<double(const FSculptBrushStamp& StampInfo, const FVector3d& Position)> FalloffFunc;

	inline double Evaluate(const FSculptBrushStamp& StampInfo, const FVector3d& Position) const
	{
		return FalloffFunc(StampInfo, Position);
	}
};



UCLASS()
class MESHMODELINGTOOLS_API UMeshSculptBrushOpProps : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:

	virtual float GetStrength() { return 1.0f; }
	virtual float GetDepth() { return 0.0f; }
	virtual float GetFalloff() { return 0.5f; }
};



class MESHMODELINGTOOLS_API FMeshSculptBrushOp
{
public:
	virtual ~FMeshSculptBrushOp() {}

	TWeakObjectPtr<UMeshSculptBrushOpProps> PropertySet;

	template<typename PropType> 
	PropType* GetPropertySetAs()
	{
		check(PropertySet.IsValid());
		return CastChecked<PropType>(PropertySet.Get());
	}


	TSharedPtr<FMeshSculptFallofFunc> Falloff;
	FSculptBrushOptions CurrentOptions;

	const FMeshSculptFallofFunc& GetFalloff() const { return *Falloff; }

	virtual void ConfigureOptions(const FSculptBrushOptions& Options)
	{
		CurrentOptions = Options;
	}

	virtual void BeginStroke(const FDynamicMesh3* Mesh, const FSculptBrushStamp& Stamp, const TArray<int32>& InitialVertices) {}
	virtual void EndStroke(const FDynamicMesh3* Mesh, const FSculptBrushStamp& Stamp, const TArray<int32>& FinalVertices) {}
	virtual void ApplyStamp(const FDynamicMesh3* Mesh, const FSculptBrushStamp& Stamp, const TArray<int32>& Vertices, TArray<FVector3d>& NewPositionsOut) = 0;



	//
	// overrideable Brush Op configuration things
	//

	virtual ESculptBrushOpTargetType GetBrushTargetType() const
	{
		return ESculptBrushOpTargetType::SculptMesh;
	}

	virtual bool GetAlignStampToView() const
	{
		return false;
	}

	virtual bool IgnoreZeroMovements() const
	{
		return false;
	}


	virtual bool WantsStampRegionPlane() const
	{
		return false;
	}
};




class MESHMODELINGTOOLS_API FMeshSculptBrushOpFactory
{
public:
	virtual ~FMeshSculptBrushOpFactory() {}
	virtual TUniquePtr<FMeshSculptBrushOp> Build() = 0;
};

template<typename OpType>
class TBasicMeshSculptBrushOpFactory : public FMeshSculptBrushOpFactory
{
public:
	virtual TUniquePtr<FMeshSculptBrushOp> Build() override
	{
		return MakeUnique<OpType>();
	}
};


class FLambdaMeshSculptBrushOpFactory : public FMeshSculptBrushOpFactory
{
public:
	TUniqueFunction<TUniquePtr<FMeshSculptBrushOp>(void)> BuildFunc;

	FLambdaMeshSculptBrushOpFactory()
	{
	}

	FLambdaMeshSculptBrushOpFactory(TUniqueFunction<TUniquePtr<FMeshSculptBrushOp>(void)> BuildFuncIn)
	{
		BuildFunc = MoveTemp(BuildFuncIn);
	}

	virtual TUniquePtr<FMeshSculptBrushOp> Build() override
	{
		return BuildFunc();
	}
};