// Copyright Epic Games, Inc. All Rights Reserved.

/*==============================================================================
NiagaraRendererSprites.h: Renderer for rendering Niagara particles as sprites.
==============================================================================*/

#pragma once

#include "NiagaraRenderer.h"
#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraSpriteVertexFactory.h"

struct FNiagaraDynamicDataSprites;

/**
* FNiagaraRendererSprites renders an FNiagaraEmitterInstance as sprite particles
*/
class NIAGARA_API FNiagaraRendererSprites : public FNiagaraRenderer
{
public:
	FNiagaraRendererSprites(ERHIFeatureLevel::Type FeatureLevel, const UNiagaraRendererProperties *InProps, const FNiagaraEmitterInstance* Emitter);
	~FNiagaraRendererSprites();

	//FNiagaraRenderer interface
	virtual void CreateRenderThreadResources(NiagaraEmitterInstanceBatcher* Batcher) override;
	virtual void ReleaseRenderThreadResources() override;

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector, const FNiagaraSceneProxy *SceneProxy) const override;
	virtual FNiagaraDynamicDataBase* GenerateDynamicData(const FNiagaraSceneProxy* Proxy, const UNiagaraRendererProperties* InProperties, const FNiagaraEmitterInstance* Emitter) const override;
	virtual int GetDynamicDataSize()const override;
	virtual bool IsMaterialValid(const UMaterialInterface* Mat)const override;

#if RHI_RAYTRACING
		virtual void GetDynamicRayTracingInstances(FRayTracingMaterialGatheringContext& Context, TArray<FRayTracingInstance>& OutRayTracingInstances, const FNiagaraSceneProxy* Proxy) final override;
#endif
	//FNiagaraRenderer interface END

private:
	struct FParticleSpriteRenderData
	{
		const FNiagaraDynamicDataSprites*	DynamicDataSprites = nullptr;
		class FNiagaraDataBuffer*			SourceParticleData = nullptr;

		bool								bHasTranslucentMaterials = false;
		bool								bSortCullOnGpu = false;
		bool								bNeedsSort = false;
		bool								bNeedsCull = false;

		const FNiagaraRendererLayout*		RendererLayout = nullptr;
		ENiagaraSpriteVFLayout::Type		SortVariable;

		FRHIShaderResourceView*				ParticleFloatSRV = nullptr;
		FRHIShaderResourceView*				ParticleHalfSRV = nullptr;
		FRHIShaderResourceView*				ParticleIntSRV = nullptr;
		uint32								ParticleFloatDataStride = 0;
		uint32								ParticleHalfDataStride = 0;
		uint32								ParticleIntDataStride = 0;

		uint32								RendererVisTagOffset = INDEX_NONE;
	};

	/* Mesh collector classes */
	class FMeshCollectorResourcesBase : public FOneFrameResource
	{
	public:
		FNiagaraSpriteUniformBufferRef UniformBuffer;

		virtual ~FMeshCollectorResourcesBase() {}
		virtual FNiagaraSpriteVertexFactory& GetVertexFactory() = 0;
	};

	template <typename TVertexFactory>
	class TMeshCollectorResources : public FMeshCollectorResourcesBase
	{
	public:
		TVertexFactory VertexFactory;

		virtual ~TMeshCollectorResources() { VertexFactory.ReleaseResource(); }
		virtual FNiagaraSpriteVertexFactory& GetVertexFactory() override { return VertexFactory; }
	};

	using FMeshCollectorResources = TMeshCollectorResources<FNiagaraSpriteVertexFactory>;
	using FMeshCollectorResourcesEx = TMeshCollectorResources<FNiagaraSpriteVertexFactoryEx>;

	void PrepareParticleSpriteRenderData(FParticleSpriteRenderData& ParticleSpriteRenderData, FNiagaraDynamicDataBase* InDynamicData, const FNiagaraSceneProxy* SceneProxy) const;
	void PrepareParticleRenderBuffers(FParticleSpriteRenderData& ParticleSpriteRenderData, FGlobalDynamicReadBuffer& DynamicReadBuffer) const;
	void InitializeSortInfo(FParticleSpriteRenderData& ParticleSpriteRenderData, const FNiagaraSceneProxy& SceneProxy, const FSceneView& View, int32 ViewIndex, FNiagaraGPUSortInfo& OutSortInfo) const;
	void SetupVertexFactory(FParticleSpriteRenderData& ParticleSpriteRenderData, FNiagaraSpriteVertexFactory& VertexFactory) const;
	FNiagaraSpriteUniformBufferRef CreateViewUniformBuffer(FParticleSpriteRenderData& ParticleSpriteRenderData, const FSceneView& View, const FSceneViewFamily& ViewFamily, const FNiagaraSceneProxy& SceneProxy, FNiagaraSpriteVertexFactory& VertexFactory) const;

	void CreateMeshBatchForView(
		FParticleSpriteRenderData& ParticleSpriteRenderData,
		FMeshBatch& MeshBatch,
		const FSceneView& View,
		const FNiagaraSceneProxy& SceneProxy,
		FNiagaraSpriteVertexFactory& VertexFactory,
		uint32 NumInstances,
		uint32 GPUCountBufferOffset,
		bool bDoGPUCulling
	) const;

	//Cached data from the properties struct.
	ENiagaraRendererSourceDataMode SourceMode;
	ENiagaraSpriteAlignment Alignment;
	ENiagaraSpriteFacingMode FacingMode;
	ENiagaraSortMode SortMode;
	FVector2D PivotInUVSpace;
	FVector2D SubImageSize;

	uint32 NumIndicesPerInstance;

	uint32 bSubImageBlend : 1;
	uint32 bRemoveHMDRollInVR : 1;
	uint32 bSortOnlyWhenTranslucent : 1;
	uint32 bGpuLowLatencyTranslucency : 1;
	uint32 bEnableCulling : 1;
	uint32 bEnableDistanceCulling : 1;
	uint32 bAccurateMotionVectors : 1;
	uint32 bSetAnyBoundVars : 1;
	uint32 bVisTagInParamStore : 1;

	float MinFacingCameraBlendDistance;
	float MaxFacingCameraBlendDistance;
	FVector2D DistanceCullRange;
	FNiagaraCutoutVertexBuffer CutoutVertexBuffer;
	int32 NumCutoutVertexPerSubImage = 0;
	uint32 MaterialParamValidMask = 0;

	int32 RendererVisTagOffset;
	int32 RendererVisibility;

	int32 VFBoundOffsetsInParamStore[ENiagaraSpriteVFLayout::Type::Num_Max];

	const FNiagaraRendererLayout* RendererLayoutWithCustomSort;
	const FNiagaraRendererLayout* RendererLayoutWithoutCustomSort;
};
