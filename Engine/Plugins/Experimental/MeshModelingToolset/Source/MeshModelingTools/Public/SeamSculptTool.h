// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DynamicMeshBrushTool.h"
#include "SeamSculptTool.generated.h"

class UExistingMeshMaterialProperties;
class UPreviewGeometry;
class FDynamicMesh3;

/**
 *
 */
UCLASS()
class MESHMODELINGTOOLS_API USeamSculptToolBuilder : public UMeshSurfacePointToolBuilder
{
	GENERATED_BODY()

public:
	virtual UMeshSurfacePointTool* CreateNewTool(const FToolBuilderState& SceneState) const override;
};





UCLASS()
class MESHMODELINGTOOLS_API USeamSculptToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere, Category = Options)
	bool bShowWireframe = false;


	UPROPERTY(EditAnywhere, Category = Options)
	bool bHitBackFaces = false;
};







UCLASS(Transient)
class MESHMODELINGTOOLS_API USeamSculptTool : public UDynamicMeshBrushTool
{
	GENERATED_BODY()
public:
	USeamSculptTool();

	virtual void SetWorld(UWorld* World);

	virtual void Setup() override;
	virtual void OnTick(float DeltaTime) override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;

	virtual bool HasCancel() const override { return true; }
	virtual bool HasAccept() const override { return true; }

	// UBaseBrushTool overrides
	virtual void OnBeginDrag(const FRay& Ray) override;
	virtual void OnUpdateDrag(const FRay& Ray) override;
	virtual void OnEndDrag(const FRay& Ray) override;
	virtual bool OnUpdateHover(const FInputDeviceRay& DevicePos) override;


protected:
	virtual void ApplyStamp(const FBrushStampData& Stamp);

	virtual void OnShutdown(EToolShutdownType ShutdownType) override;


public:
	UPROPERTY()
	USeamSculptToolProperties* Settings;


protected:
	UPROPERTY()
	UPreviewGeometry* PreviewGeom;

	TSharedPtr<FDynamicMesh3> InputMesh;
	FTransform3d MeshTransform;
	double NormalOffset = 0;

	void InitPreviewGeometry();
	void UpdatePreviewGeometry();
	bool bPreviewGeometryNeedsUpdate = false;

	FVector3d CurrentSnapPositionLocal = FVector3d::Zero();
	int32 CurrentSnapVertex = -1;

	FVector3d DrawPathStartPositionLocal = FVector3d::Zero();
	int32 DrawPathStartVertex = -1;

	TArray<int32> CurDrawPath;
	void UpdateCurrentDrawPath();


	void CreateSeamAlongPath();

	enum class EActiveCaptureState
	{
		NoState,
		DrawNewPath
	};
	EActiveCaptureState CaptureState = EActiveCaptureState::NoState;

	UWorld* TargetWorld;

};