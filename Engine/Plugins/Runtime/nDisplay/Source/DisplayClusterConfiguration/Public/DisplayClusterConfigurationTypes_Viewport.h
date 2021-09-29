// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"

#include "DisplayClusterConfigurationStrings.h"
#include "DisplayClusterConfigurationTypes_PostRender.h"
#include "DisplayClusterConfigurationTypes_Postprocess.h"

#include "DisplayClusterConfigurationTypes_TextureShare.h"

#include "OpenColorIOColorSpace.h"
#include "Engine/Scene.h"

#include "DisplayClusterConfigurationTypes_Viewport.generated.h"

USTRUCT(Blueprintable)
struct DISPLAYCLUSTERCONFIGURATION_API FDisplayClusterConfigurationViewport_Overscan
{
	GENERATED_BODY()

public:
	/** Enable/disable Viewport Overscan and specify units as percent or pixel values. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDisplay Viewport")
	EDisplayClusterConfigurationViewportOverscanMode Mode = EDisplayClusterConfigurationViewportOverscanMode::None;

	/** Left */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDisplay Viewport")
	float Left = 0;

	/** Right */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDisplay Viewport")
	float Right = 0;

	/** Top */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDisplay Viewport")
	float Top  = 0;

	/** Bottom */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDisplay Viewport")
	float Bottom = 0;

	/** Set to True to render at the overscan resolution, set to false to render at the resolution in the configuration and scale for overscan. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDisplay Viewport", meta = (DisplayName = "Adapt Resolution", DisplayAfter = "Mode"))
	bool bOversize = true;
};

USTRUCT(BlueprintType)
struct DISPLAYCLUSTERCONFIGURATION_API FDisplayClusterConfigurationViewport_ICVFX
{
	GENERATED_BODY()

public:
	/** Enable in-camera VFX for this Viewport (works only with supported Projection Policies) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "In Camera VFX")
	bool bAllowICVFX = true;

	/** Allow the inner frustum to appear on this Viewport */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "In Camera VFX")
	bool bAllowInnerFrustum = true;

	/** Disable incamera render to this viewport */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "In Camera VFX")
	EDisplayClusterConfigurationICVFX_OverrideCameraRenderMode CameraRenderMode = EDisplayClusterConfigurationICVFX_OverrideCameraRenderMode::Default;

	/** Use unique lightcard mode for this viewport */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "In Camera VFX")
	EDisplayClusterConfigurationICVFX_OverrideLightcardRenderMode LightcardRenderMode = EDisplayClusterConfigurationICVFX_OverrideLightcardRenderMode::Default;
};

USTRUCT(Blueprintable)
struct DISPLAYCLUSTERCONFIGURATION_API FDisplayClusterConfigurationViewport_RenderSettings
{
	GENERATED_BODY()

public:
	/** Specify which GPU should render the second Stereo eye */
	UPROPERTY(EditAnywhere, Category = "Configuration", meta = (DisplayName = "Stereo GPU Index"))
	int StereoGPUIndex = -1;

	/** Enables and sets Stereo mode */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
	EDisplayClusterConfigurationViewport_StereoMode StereoMode = EDisplayClusterConfigurationViewport_StereoMode::Default;

	/** Adjust resolution scaling for an individual viewport.  Viewport Screen Percentage Multiplier is applied to this value. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration", meta = (DisplayName = "Screen Percentage", ClampMin = "0.05", UIMin = "0.05", ClampMax = "10.0", UIMax = "1.0"))
	float BufferRatio = 1;

	/** Adjust resolution scaling for an individual viewport.  Viewport Screen Percentage Multiplier is applied to this value. */
	UPROPERTY(EditAnywhere, Category = "Configuration", meta = (ClampMin = "0.01", UIMin = "0.01", ClampMax = "1.0", UIMax = "1.0"))
	float RenderTargetRatio = 1.f;

	UPROPERTY()
	FDisplayClusterConfigurationViewport_CustomPostprocess CustomPostprocess;

	/** Override viewport render from source texture */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Texture Replacement")
	FDisplayClusterConfigurationPostRender_Override Replace;

	/** Add postprocess blur to viewport */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration", AdvancedDisplay)
	FDisplayClusterConfigurationPostRender_BlurPostprocess PostprocessBlur;

	/** Generate Mips texture for this viewport (used, only if projection policy supports this feature) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration", AdvancedDisplay)
	FDisplayClusterConfigurationPostRender_GenerateMips GenerateMips;

	/** Render a larger frame than specified in the configuration to achieve continuity across displays when using post-processing effects. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
	FDisplayClusterConfigurationViewport_Overscan Overscan;

	// Experimental: Support special frame builder mode - merge viewports to single viewfamily by group num
	// [not implemented yet]
	UPROPERTY()
	int RenderFamilyGroup = -1;
};

UCLASS(Blueprintable)
class DISPLAYCLUSTERCONFIGURATION_API UDisplayClusterConfigurationViewport
	: public UDisplayClusterConfigurationData_Base
{
	GENERATED_BODY()
public:

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPostEditChangeChainProperty, const FPropertyChangedChainEvent&);

	FOnPostEditChangeChainProperty OnPostEditChangeChainProperty;

public:
	UDisplayClusterConfigurationViewport();

public:
	void GetReferencedMeshNames(TArray<FString>& OutMeshNames) const;

private:
#if WITH_EDITOR
	// UObject interface
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
	// End of UObject interface
#endif

public:
	/** Enables or disables rendering of this specific Viewport */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration", meta = (DisplayName = "Enable Viewport"))
	bool bAllowRendering = true;

	/** Reference to the nDisplay View Origin */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration", meta = (DisplayName = "View Origin"))
	FString Camera;

	/** Specify your Projection Policy Settings */
	UPROPERTY(EditDefaultsOnly, Category = "Configuration")
	FDisplayClusterConfigurationProjection ProjectionPolicy;

	/** Enable or disable compatibility with inter process GPU Texture share */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Configuration", meta = (DisplayName = "Shared Texture"))
	FDisplayClusterConfigurationTextureShare_Viewport TextureShare;

#if WITH_EDITORONLY_DATA
	/** Locks the Viewport aspect ratio for easier resizing */
	UPROPERTY(EditAnywhere, Category = "Configuration")
	bool bFixedAspectRatio;
#endif
	
	/** Define the Viewport 2D coordinates */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
	FDisplayClusterConfigurationRectangle Region;

	/** Allows Viewports to overlap and sets Viewport overlapping order priority */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
	int OverlapOrder = 0;

	/** Specify which GPU should render this Viewport. "-1" is default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration", meta = (DisplayName = "GPU Index"))
	int GPUIndex = -1;

	// Configure render for this viewport
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration", meta = (ShowOnlyInnerProperties))
	FDisplayClusterConfigurationViewport_RenderSettings RenderSettings;

	// Configure ICVFX for this viewport
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "In Camera VFX", meta = (ShowOnlyInnerProperties))
	FDisplayClusterConfigurationViewport_ICVFX ICVFX;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditDefaultsOnly, Category = "Configuration", meta = (nDisplayHidden))
	bool bIsEnabled = true;

	UPROPERTY(EditDefaultsOnly, Category = "Configuration", meta = (nDisplayHidden))
	bool bIsVisible = true;
#endif

public:
	static const float ViewportMinimumSize;
	static const float ViewportMaximumSize;
};

// This struct now stored in UDisplayClusterConfigurationData, and replicated with MultiUser
USTRUCT(Blueprintable)
struct DISPLAYCLUSTERCONFIGURATION_API FDisplayClusterConfigurationRenderFrame
{
	GENERATED_BODY()

public:
	// Performance: Allow change global MGPU settings
	UPROPERTY(EditAnywhere, Category = "Configuration", meta = (DisplayName = "Multi GPU Mode"))
	EDisplayClusterConfigurationRenderMGPUMode MultiGPUMode = EDisplayClusterConfigurationRenderMGPUMode::Enabled;

	// Performance: Allow merge multiple viewports on single RTT with atlasing (required for bAllowViewFamilyMergeOptimization)
	// [not implemented yet] Experimental
	UPROPERTY()
	bool bAllowRenderTargetAtlasing = false;

	// Performance: Allow viewfamily merge optimization (render multiple viewports contexts within single family)
	// [not implemented yet] Experimental
	UPROPERTY()
	EDisplayClusterConfigurationRenderFamilyMode ViewFamilyMode = EDisplayClusterConfigurationRenderFamilyMode::None;

	// Performance: Allow to use parent ViewFamily from parent viewport 
	// (icvfx has child viewports: lightcard and chromakey with prj_view matrices copied from parent viewport. May sense to use same viewfamily?)
	// [not implemented yet] Experimental
	UPROPERTY()
	bool bShouldUseParentViewportRenderFamily = false;

	// Multiply all viewports RTT size's for whole cluster by this value
	UPROPERTY(BlueprintReadWrite, Category = NDisplay, meta = (DisplayName = "Global Viewport RTT Size Multiplier", ClampMin = "0.05", UIMin = "0.05", ClampMax = "10", UIMax = "10"))
	float ClusterRenderTargetRatioMult = 1.f;

	// Multiply inner frustum RTT size's for whole cluster by this value
	UPROPERTY(BlueprintReadWrite, Category = NDisplay, meta = (DisplayName = "Inner Frustum RTT Size Multiplier", ClampMin = "0.05", UIMin = "0.05", ClampMax = "10", UIMax = "10"))
	float ClusterICVFXInnerViewportRenderTargetRatioMult = 1.f;

	// Multiply outer viewports RTT size's for whole cluster by this value
	UPROPERTY(BlueprintReadWrite, Category = NDisplay, meta = (DisplayName = "Outer Viewport RTT Size Multiplier", ClampMin = "0.05", UIMin = "0.05", ClampMax = "10", UIMax = "10"))
	float ClusterICVFXOuterViewportRenderTargetRatioMult = 1.f;

	// Multiply all buffer ratios for whole cluster by this value
	UPROPERTY(BlueprintReadWrite, Category = NDisplay, meta = (DisplayName = "Global Viewport Screen Percentage Multiplier", ClampMin = "0.05", UIMin = "0.05", ClampMax = "10", UIMax = "10"))
	float ClusterBufferRatioMult = 1.f;

	// Multiply inner frustums buffer ratios for whole cluster by this value
	UPROPERTY(BlueprintReadWrite, Category = NDisplay, meta = (DisplayName = "Inner Frustum Screen Percentage Multiplier", ClampMin = "0.05", UIMin = "0.05", ClampMax = "10", UIMax = "10"))
	float ClusterICVFXInnerFrustumBufferRatioMult = 1.f;

	// Multiply the screen percentage for all viewports in the cluster by this value.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NDisplay, meta = (DisplayName = "Viewport Screen Percentage Multiplier", ClampMin = "0.05", UIMin = "0.05", ClampMax = "10", UIMax = "1"))
	float ClusterICVFXOuterViewportBufferRatioMult = 1.f;

	// Allow warpblend render
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
	bool bAllowWarpBlend = true;
};

USTRUCT()
struct DISPLAYCLUSTERCONFIGURATION_API FDisplayClusterConfigurationViewportPreview
{
	GENERATED_BODY()

public:
	// Allow preview render
	UPROPERTY()
	bool bEnable = true;

	// Render single node preview or whole cluster
	UPROPERTY()
	FString PreviewNodeId = DisplayClusterConfigurationStrings::gui::preview::PreviewNodeAll;

	// Update preview texture period in tick
	UPROPERTY()
	int TickPerFrame = 1;

	// Preview texture size get from viewport, and scaled by this value
	UPROPERTY()
	float PreviewRenderTargetRatioMult = 0.25;
};

