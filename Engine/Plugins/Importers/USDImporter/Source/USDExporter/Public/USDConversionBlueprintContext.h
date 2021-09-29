// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UnrealUSDWrapper.h"

#include "USDStageOptions.h"

#include "UsdWrappers/SdfLayer.h"
#include "UsdWrappers/UsdStage.h"

#include "Engine/EngineTypes.h"
#include "MaterialOptions.h"

#include "USDConversionBlueprintContext.generated.h"

class AInstancedFoliageActor;
class ALandscapeProxy;
class UCineCameraComponent;
class UDirectionalLightComponent;
class UHierarchicalInstancedStaticMeshComponent;
class ULevelExporterUSDOptions;
class ULightComponentBase;
class UMeshComponent;
class UPointLightComponent;
class URectLightComponent;
class USceneComponent;
class USkyLightComponent;
class USpotLightComponent;

/**
 * Wraps the UnrealToUsd component conversion functions from the USDUtilities module so that they can be used by
 * scripting languages.
 *
 * This is an instanceable object instead of just static functions so that the USDStage to use for the
 * conversions can be provided and cached between function calls, which is helpful because we're forced to provide
 * at most prim and layer file paths (as opposed to direct pxr::UsdPrim objects).
 *
 * We can't provide the pxr::UsdPrim object directly because USD types can't be part of C++ function signatures that
 * are automatically exposed to scripting languages. Lucikly we can use UsdUtils' stage cache to make sure that
 * C++ and e.g. Python are still referencing the same USD Stage in memory, so that we can e.g. use these functions to
 * convert data within stages created via Python.
 */
UCLASS(meta=(ScriptName="UsdConversionContext"))
class USDEXPORTER_API UUsdConversionBlueprintContext : public UObject
{
	GENERATED_BODY()

	virtual ~UUsdConversionBlueprintContext();

private:
	/** Stage to use when converting components */
    UE::FUsdStage Stage;

	/**
	 * Whether we will erase our current stage from the stage cache when we Cleanup().
	 * This is true if we were the ones that put the stage in the cache in the first place.
	 */
	bool bEraseFromStageCache = false;

public:
    /**
     * Opens or creates a USD stage using `StageRootLayerPath` as root layer, creating the root layer if needed.
     * All future conversions will fetch prims and get/set USD data to/from this stage.
	 * Note: You must remember to call Cleanup() when done, or else this object will permanently hold a reference to the opened stage!
     */
	UFUNCTION( BlueprintCallable, Category = "Export context" )
	void SetStageRootLayer( FFilePath StageRootLayerPath );

	/**
	 * Gets the file path of the root layer of our current stage
	 */
	UFUNCTION( BlueprintCallable, Category = "Export context" )
	FFilePath GetStageRootLayer();

	/**
     * Sets the current edit target of our internal stage. When calling the conversion functions, prims and attributes
	 * will be authored on this edit target only
     */
	UFUNCTION( BlueprintCallable, Category = "Export context" )
	void SetEditTarget( FFilePath EditTargetLayerPath );

	/**
	 * Gets the filepath of the current edit target layer of our internal stage
	 */
	UFUNCTION( BlueprintCallable, Category = "Export context" )
	FFilePath GetEditTarget();

	/**
	 * Discards the currently opened stage. This is critical when using this class via scripting: The C++ destructor will
	 * not be called when the python object runs out of scope, so we would otherwise keep a strong reference to the stage
	 */
	UFUNCTION( BlueprintCallable, Category = "Export context" )
	void Cleanup();

public:
	// Note: We use FLT_MAX on these functions because Usd.TimeCode.Default().GetValue() is actually a nan, and nan arguments are automatically sanitized to 0.0f.
	// We manually convert the FLT_MAX value into Usd.TimeCode.Default().GetValue() within the functions though, so if you want the Default timecode just omit the argument
	// We are also forced to copypaste the FLT_MAX value (3.402823466e+38F) in here as the default arguments are parsed before the preprocessor replaces the defines

	UFUNCTION( BlueprintCallable, Category = "Component conversion" )
	bool ConvertLightComponent( const ULightComponentBase* Component, const FString& PrimPath, float TimeCode = 3.402823466e+38F );

	UFUNCTION( BlueprintCallable, Category = "Component conversion" )
	bool ConvertDirectionalLightComponent( const UDirectionalLightComponent* Component, const FString& PrimPath, float TimeCode = 3.402823466e+38F );

	UFUNCTION( BlueprintCallable, Category = "Component conversion" )
	bool ConvertRectLightComponent( const URectLightComponent* Component, const FString& PrimPath, float TimeCode = 3.402823466e+38F );

	UFUNCTION( BlueprintCallable, Category = "Component conversion" )
	bool ConvertPointLightComponent( const UPointLightComponent* Component, const FString& PrimPath, float TimeCode = 3.402823466e+38F );

	UFUNCTION( BlueprintCallable, Category = "Component conversion" )
	bool ConvertSkyLightComponent( const USkyLightComponent* Component, const FString& PrimPath, float TimeCode = 3.402823466e+38F );

	UFUNCTION( BlueprintCallable, Category = "Component conversion" )
	bool ConvertSpotLightComponent( const USpotLightComponent* Component, const FString& PrimPath, float TimeCode = 3.402823466e+38F );

	UFUNCTION( BlueprintCallable, Category = "Component conversion" )
	bool ConvertSceneComponent( const USceneComponent* Component, const FString& PrimPath );

	UFUNCTION( BlueprintCallable, Category = "Component conversion" )
	bool ConvertHismComponent( const UHierarchicalInstancedStaticMeshComponent* Component, const FString& PrimPath, float TimeCode = 3.402823466e+38F );

	UFUNCTION( BlueprintCallable, Category = "Component conversion" )
	bool ConvertMeshComponent( const UMeshComponent* Component, const FString& PrimPath );

	UFUNCTION( BlueprintCallable, Category = "Component conversion" )
	bool ConvertCineCameraComponent( const UCineCameraComponent* Component, const FString& PrimPath );

	UFUNCTION( BlueprintCallable, Category = "Component conversion" )
	bool ConvertInstancedFoliageActor( const AInstancedFoliageActor* Actor, const FString& PrimPath, float TimeCode = 3.402823466e+38F );

	UFUNCTION( BlueprintCallable, Category = "Component conversion" )
	bool ConvertLandscapeProxyActorMesh( const ALandscapeProxy* Actor, const FString& PrimPath, int32 LowestLOD, int32 HighestLOD, float TimeCode = 3.402823466e+38F );

	UFUNCTION( BlueprintCallable, Category = "Component conversion" )
	bool ConvertLandscapeProxyActorMaterial( ALandscapeProxy* Actor, const FString& PrimPath, const TArray<FPropertyEntry>& PropertiesToBake, const FIntPoint& DefaultTextureSize, const FDirectoryPath& TexturesDir, float TimeCode = 3.402823466e+38F );
};
