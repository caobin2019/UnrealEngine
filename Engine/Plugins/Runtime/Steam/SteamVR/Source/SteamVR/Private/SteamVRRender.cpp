// Copyright Epic Games, Inc. All Rights Reserved.
//
#include "CoreMinimal.h"
#include "SteamVRPrivate.h"

#if STEAMVR_SUPPORTED_PLATFORMS

#include "SteamVRHMD.h"

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "PostProcess/PostProcessHMD.h"
#include "PipelineStateCache.h"
#include "ClearQuad.h"
#include "DefaultSpectatorScreenController.h"
#include "ScreenRendering.h"

#if PLATFORM_MAC
#include <Metal/Metal.h>
#else
#include "VulkanRHIPrivate.h"
#include "VulkanPendingState.h"
#include "VulkanContext.h"
#endif

#if PLATFORM_WINDOWS
#include "D3D12RHIPrivate.h"
#endif

static TAutoConsoleVariable<int32> CUsePostPresentHandoff(TEXT("vr.SteamVR.UsePostPresentHandoff"), 0, TEXT("Whether or not to use PostPresentHandoff.  If true, more GPU time will be available, but this relies on no SceneCaptureComponent2D or WidgetComponents being active in the scene.  Otherwise, it will break async reprojection."));

static TAutoConsoleVariable<int> CVarEnableDepthSubmission(
	TEXT("vr.EnableSteamVRDepthSubmission"),
	0,
	TEXT("By default, depth is not passed through in SteamVR for devices that support depth. Set this flag to 1 to enable depth submission, 0 to disable."),
	ECVF_Default);

void FSteamVRHMD::DrawDistortionMesh_RenderThread(struct FRenderingCompositePassContext& Context, const FIntPoint& TextureSize)
{
	check(0);
}

void FSteamVRHMD::RenderTexture_RenderThread(FRHICommandListImmediate& RHICmdList, FRHITexture2D* BackBuffer, FRHITexture2D* SrcTexture, FVector2D WindowSize) const
{
	check(IsInRenderingThread());

	if (bSplashIsShown || !IsBackgroundLayerVisible())
	{
		FRHIRenderPassInfo RPInfo(SrcTexture, ERenderTargetActions::DontLoad_Store);
		RHICmdList.BeginRenderPass(RPInfo, TEXT("Clear"));
		{
			DrawClearQuad(RHICmdList, FLinearColor(0, 0, 0, 0));
		}
		RHICmdList.EndRenderPass();
	}

	check(SpectatorScreenController);
	SpectatorScreenController->RenderSpectatorScreen_RenderThread(RHICmdList, BackBuffer, SrcTexture, WindowSize);
}

void FSteamVRHMD::PostRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView)
{
	check(IsInRenderingThread());
	UpdateStereoLayers_RenderThread();
}

bool FSteamVRHMD::IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const
{
	return GEngine && GEngine->IsStereoscopic3D(Context.Viewport) && !IsMetalPlatform(GMaxRHIShaderPlatform);
}

static void DrawOcclusionMesh(FRHICommandList& RHICmdList, EStereoscopicPass StereoPass, const FHMDViewMesh MeshAssets[])
{
	check(IsInRenderingThread());
	check(GEngine->StereoRenderingDevice->DeviceIsStereoEyePass(StereoPass));

	const uint32 MeshIndex = GEngine->StereoRenderingDevice->GetViewIndexForPass(StereoPass);
	const FHMDViewMesh& Mesh = MeshAssets[MeshIndex];
	check(Mesh.IsValid());

	RHICmdList.SetStreamSource(0, Mesh.VertexBufferRHI, 0);
	RHICmdList.DrawIndexedPrimitive(Mesh.IndexBufferRHI, 0, 0, Mesh.NumVertices, 0, Mesh.NumTriangles, 1);
}

void FSteamVRHMD::DrawHiddenAreaMesh_RenderThread(FRHICommandList& RHICmdList, EStereoscopicPass StereoPass) const
{
	DrawOcclusionMesh(RHICmdList, StereoPass, HiddenAreaMeshes);
}

void FSteamVRHMD::DrawVisibleAreaMesh_RenderThread(FRHICommandList& RHICmdList, EStereoscopicPass StereoPass) const
{
	DrawOcclusionMesh(RHICmdList, StereoPass, VisibleAreaMeshes);
}


struct FRHICommandExecute_BeginRendering final : public FRHICommand<FRHICommandExecute_BeginRendering>
{
	FSteamVRHMD::BridgeBaseImpl *pBridge;
	FRHICommandExecute_BeginRendering(FSteamVRHMD::BridgeBaseImpl* pInBridge)
		: pBridge(pInBridge)
	{
	}

	void Execute(FRHICommandListBase& /* unused */)
	{
		check(pBridge->IsUsingExplicitTimingMode());
		pBridge->BeginRendering_RHI();
	}
};

void FSteamVRHMD::BridgeBaseImpl::BeginRendering_RenderThread(FRHICommandListImmediate& RHICmdList)
{
	if (IsUsingExplicitTimingMode())
	{
		ALLOC_COMMAND_CL(RHICmdList, FRHICommandExecute_BeginRendering)(this);
	}
}

void FSteamVRHMD::BridgeBaseImpl::BeginRendering_RHI()
{
	check(!IsRunningRHIInSeparateThread() || IsInRHIThread());
	Plugin->VRCompositor->SubmitExplicitTimingData();
}

void FSteamVRHMD::BridgeBaseImpl::CreateSwapChain(const FTextureRHIRef& BindingTexture, TArray<FTextureRHIRef>&& SwapChainTextures)
{
	check(IsInRenderingThread());
	check(SwapChainTextures.Num());

	SwapChain = CreateXRSwapChain(MoveTemp(SwapChainTextures), BindingTexture);
}

void FSteamVRHMD::BridgeBaseImpl::CreateDepthSwapChain(const FTextureRHIRef& BindingTexture, TArray<FTextureRHIRef>&& SwapChainTextures)
{
	check(IsInRenderingThread());
	check(SwapChainTextures.Num());

	DepthSwapChain = CreateXRSwapChain(MoveTemp(SwapChainTextures), BindingTexture);
}

bool FSteamVRHMD::BridgeBaseImpl::Present(int& SyncInterval)
{
	check(IsRunningRHIInSeparateThread() ? IsInRHIThread() : IsInRenderingThread());

	if (Plugin->VRCompositor == nullptr)
	{
		return false;
	}

	FinishRendering();

	// Increment swap chain index post-swap.
	SwapChain->IncrementSwapChainIndex_RHIThread();
	DepthSwapChain->IncrementSwapChainIndex_RHIThread();

	SyncInterval = 0;

	return true;
}

bool FSteamVRHMD::BridgeBaseImpl::NeedsNativePresent()
{
	if (Plugin->VRCompositor == nullptr)
	{
		return false;
	}
	
	return true;
}

bool FSteamVRHMD::BridgeBaseImpl::NeedsPostPresentHandoff() const
{
	return bUseExplicitTimingMode || (CUsePostPresentHandoff.GetValueOnRenderThread() == 1);
}

void FSteamVRHMD::BridgeBaseImpl::PostPresent()
{
	if (NeedsPostPresentHandoff())
	{
		check(!IsRunningRHIInSeparateThread() || IsInRHIThread());
		Plugin->VRCompositor->PostPresentHandoff();
	}
}

#if PLATFORM_WINDOWS

FSteamVRHMD::D3D11Bridge::D3D11Bridge(FSteamVRHMD* plugin)
	: BridgeBaseImpl(plugin)
{
}

void FSteamVRHMD::D3D11Bridge::FinishRendering()
{
	bool bSubmitDepth = CVarEnableDepthSubmission->GetInt() > 0;
	vr::EVRSubmitFlags Flags = bSubmitDepth ? vr::EVRSubmitFlags::Submit_TextureWithDepth : vr::EVRSubmitFlags::Submit_Default;

	vr::VRTextureWithDepth_t Texture;
	Texture.handle = SwapChain->GetTexture2D()->GetNativeResource();
	Texture.eType = vr::TextureType_DirectX;
	Texture.eColorSpace = vr::ColorSpace_Auto;

	if (bSubmitDepth)
	{
		// If this flag is false, the struct will be treated as a vr::Texture_t and these entries will be ignored - so we can skip this if not submitting depth
		Texture.depth.handle = DepthSwapChain->GetTexture2D()->GetNativeResource();

		// Set the texture depth range to follow Unreal's inverted-Z depth settings (near is 1.0f, far is 0.0f).
		Texture.depth.vRange.v[0] = 1.0f;
		Texture.depth.vRange.v[1] = 0.0f;

		Texture.depth.mProjection = ToHmdMatrix44(Plugin->GetStereoProjectionMatrix(eSSP_LEFT_EYE));
	}

	vr::VRTextureBounds_t LeftBounds;
	LeftBounds.uMin = 0.0f;
	LeftBounds.uMax = 0.5f;
	LeftBounds.vMin = 0.0f;
	LeftBounds.vMax = 1.0f;

	vr::EVRCompositorError Error = Plugin->VRCompositor->Submit(vr::Eye_Left, &Texture, &LeftBounds, Flags);

	vr::VRTextureBounds_t RightBounds;
	RightBounds.uMin = 0.5f;
	RightBounds.uMax = 1.0f;
	RightBounds.vMin = 0.0f;
	RightBounds.vMax = 1.0f;

	if (bSubmitDepth)
	{
		Texture.depth.mProjection = ToHmdMatrix44(Plugin->GetStereoProjectionMatrix(eSSP_RIGHT_EYE));
	}

	Error = Plugin->VRCompositor->Submit(vr::Eye_Right, &Texture, &RightBounds, Flags);

	static bool FirstError = true;
	if (FirstError && Error != vr::VRCompositorError_None)
	{
		UE_LOG(LogHMD, Log, TEXT("Warning: SteamVR Compositor had an error on present (%d)"), (int32)Error);
		FirstError = false;
	}
}

void FSteamVRHMD::D3D11Bridge::Reset()
{
}

void FSteamVRHMD::D3D11Bridge::UpdateViewport(const FViewport& Viewport, FRHIViewport* InViewportRHI)
{
	check(IsInGameThread());
	check(InViewportRHI);

	const FTexture2DRHIRef& RT = Viewport.GetRenderTargetTexture();
	check(IsValidRef(RT));
}

FSteamVRHMD::D3D12Bridge::D3D12Bridge(FSteamVRHMD* plugin)
	: BridgeBaseImpl(plugin)
{
	bUseExplicitTimingMode = true;
}

void FSteamVRHMD::D3D12Bridge::FinishRendering()
{
	bool bSubmitDepth = CVarEnableDepthSubmission->GetInt() > 0;
	vr::EVRSubmitFlags Flags = bSubmitDepth ? vr::EVRSubmitFlags::Submit_TextureWithDepth : vr::EVRSubmitFlags::Submit_Default;

	FD3D12DynamicRHI* D3D12RHI = FD3D12DynamicRHI::GetD3DRHI();
	FD3D12Device* Device = D3D12RHI->GetAdapter().GetDevice(0);

	vr::D3D12TextureData_t TextureData;
	TextureData.m_pResource = (ID3D12Resource*)SwapChain->GetTexture2D()->GetNativeResource();
	TextureData.m_pCommandQueue = D3D12RHI->RHIGetD3DCommandQueue();
	TextureData.m_nNodeMask = Device->GetGPUMask().GetNative();

	vr::VRTextureWithDepth_t Texture;
	Texture.handle = &TextureData;
	Texture.eType = vr::TextureType_DirectX12;
	Texture.eColorSpace = vr::ColorSpace_Auto;

	// Need function scope here since we use the pointer to this is the handle for the depth texture in the struct below.
	vr::D3D12TextureData_t DepthTextureData;

	if (bSubmitDepth)
	{
		// If this flag is false, the struct will be treated as a vr::Texture_t and these entries will be ignored - so we can skip this if not submitting depth
		DepthTextureData.m_pResource = (ID3D12Resource*)DepthSwapChain->GetTexture2D()->GetNativeResource();
		DepthTextureData.m_pCommandQueue = D3D12RHI->RHIGetD3DCommandQueue();
		DepthTextureData.m_nNodeMask = Device->GetGPUMask().GetNative();

		Texture.depth.handle = &DepthTextureData;

		// Set the texture depth range to follow Unreal's inverted-Z depth settings (near is 1.0f, far is 0.0f).
		Texture.depth.vRange.v[0] = 1.0f;
		Texture.depth.vRange.v[1] = 0.0f;

		Texture.depth.mProjection = ToHmdMatrix44(Plugin->GetStereoProjectionMatrix(eSSP_LEFT_EYE));
		// Rescale the projection (our projection value is 10.0f here, since our units are cm, and SteamVR works in meters).
		Texture.depth.mProjection.m[2][3] *= 0.01f;
	}

	vr::VRTextureBounds_t LeftBounds;
	LeftBounds.uMin = 0.0f;
	LeftBounds.uMax = 0.5f;
	LeftBounds.vMin = 0.0f;
	LeftBounds.vMax = 1.0f;

	vr::EVRCompositorError Error = Plugin->VRCompositor->Submit(vr::Eye_Left, &Texture, &LeftBounds, Flags);

	vr::VRTextureBounds_t RightBounds;
	RightBounds.uMin = 0.5f;
	RightBounds.uMax = 1.0f;
	RightBounds.vMin = 0.0f;
	RightBounds.vMax = 1.0f;

	if (bSubmitDepth)
	{
		Texture.depth.mProjection = ToHmdMatrix44(Plugin->GetStereoProjectionMatrix(eSSP_RIGHT_EYE));
		// Rescale the projection (our projection value is 10.0f here, since our units are cm, and SteamVR works in meters).
		Texture.depth.mProjection.m[2][3] *= 0.01f;
	}

	Error = Plugin->VRCompositor->Submit(vr::Eye_Right, &Texture, &RightBounds, Flags);

	static bool FirstError = true;
	if (FirstError && Error != vr::VRCompositorError_None)
	{
		UE_LOG(LogHMD, Log, TEXT("Warning: SteamVR Compositor had an error on present (%d)"), (int32)Error);
		FirstError = false;
	}
}

void FSteamVRHMD::D3D12Bridge::UpdateViewport(const FViewport& Viewport, FRHIViewport* InViewportRHI)
{
	check(IsInGameThread());
	check(InViewportRHI);

	const FTexture2DRHIRef& RT = Viewport.GetRenderTargetTexture();
	check(IsValidRef(RT));
	check(RT->GetTexture2D() == SwapChain->GetTexture2D());
}

void FSteamVRHMD::D3D12Bridge::Reset()
{
}

#endif // PLATFORM_WINDOWS

#if !PLATFORM_MAC
FSteamVRHMD::VulkanBridge::VulkanBridge(FSteamVRHMD* plugin):
	BridgeBaseImpl(plugin)
{
	bInitialized = true;
	bUseExplicitTimingMode = true;
}

void FSteamVRHMD::VulkanBridge::FinishRendering()
{
	// Disable depth-submission until Vulkan "device lost" error on submission is tracked down.
	bool bSubmitDepth = false; // CVarEnableDepthSubmission->GetInt() > 0;
	vr::EVRSubmitFlags Flags = bSubmitDepth ? vr::EVRSubmitFlags::Submit_TextureWithDepth : vr::EVRSubmitFlags::Submit_Default;

	if(SwapChain->GetTexture2D())
	{
		FVulkanTexture2D* Texture2D = (FVulkanTexture2D*)SwapChain->GetTexture2D();
		FVulkanTexture2D* DepthTexture2D = (FVulkanTexture2D*)DepthSwapChain->GetTexture2D();

		FVulkanCommandListContext& ImmediateContext = GVulkanRHI->GetDevice()->GetImmediateContext();

		// Color layout
		VkImageLayout& CurrentLayout = ImmediateContext.GetLayoutManager().FindOrAddLayoutRW(Texture2D->Surface, VK_IMAGE_LAYOUT_UNDEFINED);
		bool bHadLayout = (CurrentLayout != VK_IMAGE_LAYOUT_UNDEFINED);
		
		FVulkanCmdBuffer* CmdBuffer = ImmediateContext.GetCommandBufferManager()->GetUploadCmdBuffer();
		VkImageSubresourceRange SubresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

		if (CurrentLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
		{
			GVulkanRHI->VulkanSetImageLayout(CmdBuffer->GetHandle(), Texture2D->Surface.Image, CurrentLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, SubresourceRange);
		}

		vr::VRTextureBounds_t LeftBounds;
		LeftBounds.uMin = 0.0f;
		LeftBounds.uMax = 0.5f;
		LeftBounds.vMin = 0.0f;
		LeftBounds.vMax = 1.0f;

		vr::VRTextureBounds_t RightBounds;
		RightBounds.uMin = 0.5f;
		RightBounds.uMax = 1.0f;
		RightBounds.vMin = 0.0f;
		RightBounds.vMax = 1.0f;

		vr::VRVulkanTextureData_t VulkanTextureDataColor {};
		VulkanTextureDataColor.m_pInstance			= GVulkanRHI->GetInstance();
		VulkanTextureDataColor.m_pDevice			= GVulkanRHI->GetDevice()->GetInstanceHandle();
		VulkanTextureDataColor.m_pPhysicalDevice	= GVulkanRHI->GetDevice()->GetPhysicalHandle();
		VulkanTextureDataColor.m_pQueue				= GVulkanRHI->GetDevice()->GetGraphicsQueue()->GetHandle();
		VulkanTextureDataColor.m_nQueueFamilyIndex	= GVulkanRHI->GetDevice()->GetGraphicsQueue()->GetFamilyIndex();
		VulkanTextureDataColor.m_nImage				= (uint64_t)Texture2D->Surface.Image;
		VulkanTextureDataColor.m_nWidth				= Texture2D->Surface.Width;
		VulkanTextureDataColor.m_nHeight			= Texture2D->Surface.Height;
		VulkanTextureDataColor.m_nFormat			= (uint32_t)Texture2D->Surface.ViewFormat;
		VulkanTextureDataColor.m_nSampleCount = 1;

		if (bSubmitDepth)
		{
			VkImageSubresourceRange SubresourceRangeDepth = { VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1 };
			VkImageLayout& CurrentDepthLayout = ImmediateContext.GetLayoutManager().FindOrAddLayoutRW(DepthTexture2D->Surface, VK_IMAGE_LAYOUT_UNDEFINED);
			bool bDepthHadLayout = (CurrentDepthLayout != VK_IMAGE_LAYOUT_UNDEFINED);

			if (CurrentDepthLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
			{
				GVulkanRHI->VulkanSetImageLayout(CmdBuffer->GetHandle(), DepthTexture2D->Surface.Image, CurrentDepthLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, SubresourceRangeDepth);
			}

			vr::VRVulkanTextureData_t VulkanTextureDataDepth{};
			VulkanTextureDataDepth.m_pInstance = GVulkanRHI->GetInstance();
			VulkanTextureDataDepth.m_pDevice = GVulkanRHI->GetDevice()->GetInstanceHandle();
			VulkanTextureDataDepth.m_pPhysicalDevice = GVulkanRHI->GetDevice()->GetPhysicalHandle();
			VulkanTextureDataDepth.m_pQueue = GVulkanRHI->GetDevice()->GetGraphicsQueue()->GetHandle();
			VulkanTextureDataDepth.m_nQueueFamilyIndex = GVulkanRHI->GetDevice()->GetGraphicsQueue()->GetFamilyIndex();
			VulkanTextureDataDepth.m_nImage = (uint64_t)DepthTexture2D->Surface.Image;
			VulkanTextureDataDepth.m_nWidth = DepthTexture2D->Surface.Width;
			VulkanTextureDataDepth.m_nHeight = DepthTexture2D->Surface.Height;
			VulkanTextureDataDepth.m_nFormat = (uint32_t)DepthTexture2D->Surface.ViewFormat;
			VulkanTextureDataDepth.m_nSampleCount = 1;

			vr::VRTextureWithDepth_t Texture;
			Texture.handle = &VulkanTextureDataColor;
			Texture.eColorSpace = vr::ColorSpace_Auto;
			Texture.eType = vr::TextureType_Vulkan;
			Texture.depth.handle = &VulkanTextureDataDepth;
			Texture.depth.vRange.v[0] = 1.0f;
			Texture.depth.vRange.v[1] = 0.0f;

			Texture.depth.mProjection = ToHmdMatrix44(Plugin->GetStereoProjectionMatrix(eSSP_LEFT_EYE));

			// Rescale the projection (our projection value is 10.0f here, since our units are cm, and SteamVR works in meters).
			Texture.depth.mProjection.m[2][3] *= 0.01f;

			Plugin->VRCompositor->Submit(vr::Eye_Left, &Texture, &LeftBounds, vr::EVRSubmitFlags::Submit_TextureWithDepth);

			Texture.depth.mProjection = ToHmdMatrix44(Plugin->GetStereoProjectionMatrix(eSSP_RIGHT_EYE));

			// Rescale the projection (our projection value is 10.0f here, since our units are cm, and SteamVR works in meters).
			Texture.depth.mProjection.m[2][3] *= 0.01f;

			Plugin->VRCompositor->Submit(vr::Eye_Right, &Texture, &RightBounds, vr::EVRSubmitFlags::Submit_TextureWithDepth);

			if (bDepthHadLayout && CurrentDepthLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
			{
				GVulkanRHI->VulkanSetImageLayout(CmdBuffer->GetHandle(), DepthTexture2D->Surface.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, CurrentDepthLayout, SubresourceRangeDepth);
			}
			else
			{
				CurrentDepthLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			}
		}
		else
		{
			vr::Texture_t Texture;
			Texture.handle = &VulkanTextureDataColor;
			Texture.eColorSpace = vr::ColorSpace_Auto;
			Texture.eType = vr::TextureType_Vulkan;

			Plugin->VRCompositor->Submit(vr::Eye_Left, &Texture, &LeftBounds, vr::EVRSubmitFlags::Submit_Default);
			Plugin->VRCompositor->Submit(vr::Eye_Right, &Texture, &RightBounds, vr::EVRSubmitFlags::Submit_Default);
		}

		if (bHadLayout && CurrentLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
		{
			GVulkanRHI->VulkanSetImageLayout(CmdBuffer->GetHandle(), Texture2D->Surface.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, CurrentLayout, SubresourceRange);
		}
		else
		{
			CurrentLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		}

		ImmediateContext.GetCommandBufferManager()->SubmitUploadCmdBuffer();
	}
}

void FSteamVRHMD::VulkanBridge::Reset()
{
}

void FSteamVRHMD::VulkanBridge::UpdateViewport(const FViewport& Viewport, FRHIViewport* InViewportRHI)
{
}

FSteamVRHMD::OpenGLBridge::OpenGLBridge(FSteamVRHMD* plugin):
	BridgeBaseImpl(plugin)
{
	bInitialized = true;
}

void FSteamVRHMD::OpenGLBridge::FinishRendering()
{
	GLuint RenderTargetTexture = *reinterpret_cast<GLuint*>(SwapChain->GetTexture2D()->GetNativeResource());
	GLuint DepthTargetTexture = *reinterpret_cast<GLuint*>(DepthSwapChain->GetTexture2D()->GetNativeResource());

	// Yaakuro:
	// TODO This is a workaround. After exiting VR Editor the texture gets invalid at some point.
	// Need to find it. This at least prevents to use this method when texture name is not valid anymore.
	if (!glIsTexture(RenderTargetTexture) || !glIsTexture(DepthTargetTexture))
	{
		return;
	}

	vr::VRTextureBounds_t LeftBounds;
	LeftBounds.uMin = 0.0f;
	LeftBounds.uMax = 0.5f;
	LeftBounds.vMin = 1.0f;
	LeftBounds.vMax = 0.0f;

	vr::VRTextureBounds_t RightBounds;
	RightBounds.uMin = 0.5f;
	RightBounds.uMax = 1.0f;
	RightBounds.vMin = 1.0f;
	RightBounds.vMax = 0.0f;

	vr::VRTextureWithDepth_t Texture;
	Texture.handle = reinterpret_cast<void*>(static_cast<size_t>(RenderTargetTexture));
	Texture.eType = vr::TextureType_OpenGL;
	Texture.eColorSpace = vr::ColorSpace_Auto;
	Texture.depth.handle = reinterpret_cast<void*>(static_cast<size_t>(DepthTargetTexture));
	Texture.depth.vRange.v[0] = 1.0f;
	Texture.depth.vRange.v[1] = 0.0f;

	Texture.depth.mProjection = ToHmdMatrix44(Plugin->GetStereoProjectionMatrix(eSSP_LEFT_EYE));
	Plugin->VRCompositor->Submit(vr::Eye_Left, &Texture, &LeftBounds, vr::EVRSubmitFlags::Submit_TextureWithDepth);

	Texture.depth.mProjection = ToHmdMatrix44(Plugin->GetStereoProjectionMatrix(eSSP_RIGHT_EYE));
	Plugin->VRCompositor->Submit(vr::Eye_Right, &Texture, &RightBounds, vr::EVRSubmitFlags::Submit_TextureWithDepth);
}

void FSteamVRHMD::OpenGLBridge::Reset()
{
}

void FSteamVRHMD::OpenGLBridge::UpdateViewport(const FViewport& Viewport, FRHIViewport* InViewportRHI)
{
	check(IsInGameThread());

	const FTexture2DRHIRef& RT = Viewport.GetRenderTargetTexture();
	check(IsValidRef(RT));
	check(RT == SwapChain->GetTexture2D());
}

#elif PLATFORM_MAC

FSteamVRHMD::MetalBridge::MetalBridge(FSteamVRHMD* plugin):
	BridgeBaseImpl(plugin)
{}

void FSteamVRHMD::MetalBridge::FinishRendering()
{
	vr::VRTextureBounds_t LeftBounds;
	LeftBounds.uMin = 0.0f;
	LeftBounds.uMax = 0.5f;
	LeftBounds.vMin = 0.0f;
	LeftBounds.vMax = 1.0f;
	
	id<MTLTexture> TextureHandle = (id<MTLTexture>)SwapChain->GetTexture2D()->GetNativeResource();

	// @todo: Add depth.
	vr::Texture_t Texture;
	Texture.handle = (void*)TextureHandle.iosurface;
	Texture.eType = vr::TextureType_IOSurface;
	Texture.eColorSpace = vr::ColorSpace_Auto;

	vr::EVRCompositorError Error = Plugin->VRCompositor->Submit(vr::Eye_Left, &Texture, &LeftBounds);

	vr::VRTextureBounds_t RightBounds;
	RightBounds.uMin = 0.5f;
	RightBounds.uMax = 1.0f;
	RightBounds.vMin = 0.0f;
	RightBounds.vMax = 1.0f;
	
	Error = Plugin->VRCompositor->Submit(vr::Eye_Right, &Texture, &RightBounds);

	static bool FirstError = true;
	if (FirstError && Error != vr::VRCompositorError_None)
	{
		UE_LOG(LogHMD, Log, TEXT("Warning:  SteamVR Compositor had an error on present (%d)"), (int32)Error);
		FirstError = false;
	}
}

void FSteamVRHMD::MetalBridge::Reset()
{
}

IOSurfaceRef FSteamVRHMD::MetalBridge::GetSurface(const uint32 SizeX, const uint32 SizeY)
{
	// @todo: Get our man in MacVR to switch to a modern & secure method of IOSurface sharing...
	// @todo: Also add support for depth.
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	const NSDictionary* SurfaceDefinition = @{
											(id)kIOSurfaceWidth: @(SizeX),
											(id)kIOSurfaceHeight: @(SizeY),
											(id)kIOSurfaceBytesPerElement: @(4), // sizeof(PF_B8G8R8A8)..
											(id)kIOSurfaceIsGlobal: @YES
											};
	
	return IOSurfaceCreate((CFDictionaryRef)SurfaceDefinition);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

#endif // PLATFORM_MAC

#endif // STEAMVR_SUPPORTED_PLATFORMS
