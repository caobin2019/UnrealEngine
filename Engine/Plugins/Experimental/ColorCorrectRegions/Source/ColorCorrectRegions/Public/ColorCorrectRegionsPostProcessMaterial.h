// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "UObject/ConstructorHelpers.h"
#include "RHI.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"
#include "Runtime/RHI/Public/RHIStaticStates.h"
#include "Runtime/RenderCore/Public/ShaderParameterUtils.h"
#include "Runtime/RenderCore/Public/RenderResource.h"
#include "Runtime/Renderer/Public/MaterialShader.h"
#include "Runtime/RenderCore/Public/RenderGraphResources.h"
#include "Runtime/RenderCore/Public/RenderGraphResources.h"
#include "ColorCorrectRegion.h"

// FScreenPassTextureViewportParameters and FScreenPassTextureInput
#include "Runtime/Renderer/Private/ScreenPass.h"
#include "Runtime/Renderer/Private/SceneTextureParameters.h"



BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FCCRRegionDataInputParameter, )
	SHADER_PARAMETER(FVector, Rotate)
	SHADER_PARAMETER(FVector, Translate)
	SHADER_PARAMETER(FVector, Scale)

	SHADER_PARAMETER(float, WhiteTemp)
	SHADER_PARAMETER(float, Inner)
	SHADER_PARAMETER(float, Outer)
	SHADER_PARAMETER(float, Falloff)
	SHADER_PARAMETER(float, Intensity)
	SHADER_PARAMETER(float, FakeLight)
	SHADER_PARAMETER(float, ExcludeStencil)
	SHADER_PARAMETER(float, Invert)
END_GLOBAL_SHADER_PARAMETER_STRUCT()


BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FCCRColorCorrectParameter, )
	SHADER_PARAMETER(FVector4, ColorSaturation)
	SHADER_PARAMETER(FVector4, ColorContrast)
	SHADER_PARAMETER(FVector4, ColorGamma)
	SHADER_PARAMETER(FVector4, ColorGain)
	SHADER_PARAMETER(FVector4, ColorOffset)
END_GLOBAL_SHADER_PARAMETER_STRUCT()


BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FCCRColorCorrectShadowsParameter, )
	SHADER_PARAMETER(FVector4, ColorSaturation)
	SHADER_PARAMETER(FVector4, ColorContrast)
	SHADER_PARAMETER(FVector4, ColorGamma)
	SHADER_PARAMETER(FVector4, ColorGain)
	SHADER_PARAMETER(FVector4, ColorOffset)
	SHADER_PARAMETER(float, ShadowMax)
END_GLOBAL_SHADER_PARAMETER_STRUCT()


BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FCCRColorCorrectMidtonesParameter, )
	SHADER_PARAMETER(FVector4, ColorSaturation)
	SHADER_PARAMETER(FVector4, ColorContrast)
	SHADER_PARAMETER(FVector4, ColorGamma)
	SHADER_PARAMETER(FVector4, ColorGain)
	SHADER_PARAMETER(FVector4, ColorOffset)
END_GLOBAL_SHADER_PARAMETER_STRUCT()


BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FCCRColorCorrectHighlightsParameter, )
	SHADER_PARAMETER(FVector4, ColorSaturation)
	SHADER_PARAMETER(FVector4, ColorContrast)
	SHADER_PARAMETER(FVector4, ColorGamma)
	SHADER_PARAMETER(FVector4, ColorGain)
	SHADER_PARAMETER(FVector4, ColorOffset)
	SHADER_PARAMETER(float, HighlightsMin)
END_GLOBAL_SHADER_PARAMETER_STRUCT()


BEGIN_SHADER_PARAMETER_STRUCT(FCCRShaderInputParameters, )
	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
	SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureShaderParameters, SceneTextures)
	SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, PostProcessOutput)
	SHADER_PARAMETER_STRUCT_ARRAY(FScreenPassTextureInput, PostProcessInput, [1])
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()


class FColorCorrectRegionsPostProcessMaterialShader : public FGlobalShader
{
public:
	using FParameters = FCCRShaderInputParameters;
	SHADER_USE_PARAMETER_STRUCT_WITH_LEGACY_BASE(FColorCorrectRegionsPostProcessMaterialShader, FGlobalShader);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return !IsMobilePlatform(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}

};

class FColorCorrectRegionMaterialVS : public FColorCorrectRegionsPostProcessMaterialShader
{
public:
	DECLARE_GLOBAL_SHADER(FColorCorrectRegionMaterialVS);

	FColorCorrectRegionMaterialVS() = default;
	FColorCorrectRegionMaterialVS(const ShaderMetaType::CompiledShaderInitializerType & Initializer)
		: FColorCorrectRegionsPostProcessMaterialShader(Initializer)
	{}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View)
	{
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, RHICmdList.GetBoundVertexShader(), View.ViewUniformBuffer);
	}
	
};

class FColorCorrectRegionMaterialPS : public FColorCorrectRegionsPostProcessMaterialShader
{
public:
	UENUM(BlueprintType)
	enum class ETemperatureType : uint8
	{
		LegacyTemperature = static_cast<uint8>(EColorCorrectRegionTemperatureType::LegacyTemperature),
		WhiteBalance = static_cast<uint8>(EColorCorrectRegionTemperatureType::WhiteBalance),
		ColorTemperature = static_cast<uint8>(EColorCorrectRegionTemperatureType::ColorTemperature),
		Disabled,
		MAX
	};
	DECLARE_GLOBAL_SHADER(FColorCorrectRegionMaterialPS);

	class FShaderType : SHADER_PERMUTATION_ENUM_CLASS("SHAPE_TYPE", EColorCorrectRegionsType);
	class FTemperatureType : SHADER_PERMUTATION_ENUM_CLASS("TEMPERATURE_TYPE", ETemperatureType);
	class FAdvancedShader : SHADER_PERMUTATION_BOOL("ADVANCED_CC");
	class FDisplayBoundingRect : SHADER_PERMUTATION_BOOL("CCR_SHADER_DISPLAY_BOUNDING_RECT");
	class FClipPixelsOutsideAABB : SHADER_PERMUTATION_BOOL("CLIP_PIXELS_OUTSIDE_AABB");

	/** 
	* On lower scalability settings Scene texture has only 3 channels.
	* Which means we cannot sample it for opacity and need to get it from a different source.
	*/
	class FSampleOpacityFromGbuffer : SHADER_PERMUTATION_BOOL("SAMPLE_OPACITY_FROM_GBUFFER");

	using FPermutationDomain = TShaderPermutationDomain<FShaderType, FTemperatureType, FAdvancedShader, FDisplayBoundingRect, FClipPixelsOutsideAABB, FSampleOpacityFromGbuffer>;

	FColorCorrectRegionMaterialPS() = default;
	FColorCorrectRegionMaterialPS(const ShaderMetaType::CompiledShaderInitializerType & Initializer)
		: FColorCorrectRegionsPostProcessMaterialShader(Initializer)
	{}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View)
	{
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, RHICmdList.GetBoundPixelShader(), View.ViewUniformBuffer);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment & OutEnvironment)
	{
		FColorCorrectRegionsPostProcessMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}
};

// The vertex shader used by DrawScreenPass to draw a rectangle.
class FColorCorrectScreenPassVS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FColorCorrectScreenPassVS);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters&)
	{
		return true;
	}

	FColorCorrectScreenPassVS() = default;
	FColorCorrectScreenPassVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{}
};

// A simple shader that outputs (0.,0.,0.,0.)
class FClearRectPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FClearRectPS);
	SHADER_USE_PARAMETER_STRUCT(FClearRectPS, FGlobalShader);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) 
	{
		return true;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FColorCorrectRegionMaterialPS::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("CLEAR_PS"), 1);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};
