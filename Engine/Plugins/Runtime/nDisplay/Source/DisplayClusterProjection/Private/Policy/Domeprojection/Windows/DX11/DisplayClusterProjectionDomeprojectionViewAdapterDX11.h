// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Policy/Domeprojection/DisplayClusterProjectionDomeprojectionViewAdapterBase.h"

THIRD_PARTY_INCLUDES_START
#include "dpTypes.h"
THIRD_PARTY_INCLUDES_END


class FDisplayClusterProjectionDomeprojectionViewAdapterDX11
	: public FDisplayClusterProjectionDomeprojectionViewAdapterBase
{
public:
	FDisplayClusterProjectionDomeprojectionViewAdapterDX11(const FDisplayClusterProjectionDomeprojectionViewAdapterBase::FInitParams& InitParams);
	virtual ~FDisplayClusterProjectionDomeprojectionViewAdapterDX11();

public:
	virtual bool Initialize(const FString& File) override;

public:
	virtual bool CalculateView(class IDisplayClusterViewport* InViewport, const uint32 InContextNum, const uint32 Channel, FVector& InOutViewLocation, FRotator& InOutViewRotation, const FVector& ViewOffset, const float WorldToMeters, const float NCP, const float FCP) override;
	virtual bool GetProjectionMatrix(class IDisplayClusterViewport* InViewport, const uint32 InContextNum, const uint32 Channel, FMatrix& OutPrjMatrix) override;
	virtual bool ApplyWarpBlend_RenderThread(FRHICommandListImmediate& RHICmdList, const class IDisplayClusterViewportProxy* InViewportProxy, const uint32 Channel) override;

protected:
	bool ImplApplyWarpBlend_RenderThread(FRHICommandListImmediate& RHICmdList, int ContextNum, FRHITexture2D* InputTextures, FRHITexture2D* OutputTextures);

private:
	float ZNear;
	float ZFar;

	class FViewData
	{
	public:
		FViewData() = default;
		~FViewData()
		{ Release(); }

		bool Initialize(const FString& InFile);
		void Release();
	public:
		dpCamera   Camera;
		// unique context for each eye (hold warp settings, different for each eye)
		dpContext* Context = nullptr;
	};

	TArray<FViewData> Views;

	FCriticalSection DllAccessCS;
};
