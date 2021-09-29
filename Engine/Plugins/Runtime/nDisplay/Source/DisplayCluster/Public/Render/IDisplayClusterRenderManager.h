// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Render/Device/IDisplayClusterRenderDevice.h"

class IDisplayClusterRenderDevice;
class IDisplayClusterPostProcess;
class IDisplayClusterPostProcessFactory;
class IDisplayClusterProjectionPolicy;
class IDisplayClusterProjectionPolicyFactory;
class IDisplayClusterRenderDeviceFactory;
class IDisplayClusterRenderSyncPolicy;
class IDisplayClusterRenderSyncPolicyFactory;
class IDisplayClusterViewportManager; 
class UCineCameraComponent;
struct FPostProcessSettings;


/**
 * Public render manager interface
 */
class IDisplayClusterRenderManager
{
public:
	virtual ~IDisplayClusterRenderManager() = default;

public:
	// Post-process operation wrapper
	struct FDisplayClusterPPInfo
	{
		FDisplayClusterPPInfo(TSharedPtr<IDisplayClusterPostProcess, ESPMode::ThreadSafe>& InOperation, int InPriority)
			: Operation(InOperation)
			, Priority(InPriority)
		{ }

		TSharedPtr<IDisplayClusterPostProcess, ESPMode::ThreadSafe> Operation;
		int Priority;
	};

public:
	/**
	* Returns current rendering device
	*
	* @return - nullptr if failed
	*/
	virtual IDisplayClusterRenderDevice* GetRenderDevice() const = 0;

	/**
	* Registers rendering device factory
	*
	* @param DeviceType - Type of rendering device
	* @param Factory    - Factory instance
	*
	* @return - True if success
	*/
	virtual bool RegisterRenderDeviceFactory(const FString& DeviceType, TSharedPtr<IDisplayClusterRenderDeviceFactory>& Factory) = 0;

	/**
	* Unregisters rendering device factory
	*
	* @param DeviceType - Type of rendering device
	*
	* @return - True if success
	*/
	virtual bool UnregisterRenderDeviceFactory(const FString& DeviceType) = 0;

	/**
	* Registers synchronization policy factory
	*
	* @param SyncPolicyType - Type of synchronization policy
	* @param Factory        - Factory instance
	*
	* @return - True if success
	*/
	virtual bool RegisterSynchronizationPolicyFactory(const FString& SyncPolicyType, TSharedPtr<IDisplayClusterRenderSyncPolicyFactory>& Factory) = 0;

	/**
	* Unregisters synchronization policy factory
	*
	* @param SyncPolicyType - Type of synchronization policy
	*
	* @return - True if success
	*/
	virtual bool UnregisterSynchronizationPolicyFactory(const FString& SyncPolicyType) = 0;

	/**
	* Returns currently active rendering synchronization policy object
	*
	* @return - Rendering synchronization policy object
	*/
	virtual TSharedPtr<IDisplayClusterRenderSyncPolicy> GetCurrentSynchronizationPolicy() = 0;

	/**
	* Registers projection policy factory
	*
	* @param ProjectionType - Type of projection data (MPCDI etc.)
	* @param Factory        - Factory instance
	*
	* @return - True if success
	*/
	virtual bool RegisterProjectionPolicyFactory(const FString& ProjectionType, TSharedPtr<IDisplayClusterProjectionPolicyFactory>& Factory) = 0;

	/**
	* Unregisters projection policy factory
	*
	* @param ProjectionType - Type of projection policy
	*
	* @return - True if success
	*/
	virtual bool UnregisterProjectionPolicyFactory(const FString& ProjectionType) = 0;

	/**
	* Returns a projection policy factory of specified type (if it has been registered previously)
	*
	* @param ProjectionType - Projection policy type
	*
	* @return - Projection policy factory pointer or nullptr if not registered
	*/
	virtual TSharedPtr<IDisplayClusterProjectionPolicyFactory> GetProjectionPolicyFactory(const FString& ProjectionType) = 0;

	/**
	* Returns all registered projection policy types
	*
	* @param OutPolicyIDs - (out) array to put registered IDs
	*/
	virtual void GetRegisteredProjectionPolicies(TArray<FString>& OutPolicyIDs) const = 0;

	/**
	* Registers PostProcess factory
	*
	* @param PostProcessType - Type of PostProcess data
	* @param Factory        - Factory instance
	*
	* @return - True if success
	*/
	virtual bool RegisterPostProcessFactory(const FString& PostProcessType, TSharedPtr<IDisplayClusterPostProcessFactory>& Factory) = 0;

	/**
	* Unregisters PostProcess factory
	*
	* @param PostProcessType - Type of post process
	*
	* @return - True if success
	*/
	virtual bool UnregisterPostProcessFactory(const FString& PostProcessType) = 0;

	/**
	* Returns a postprocess factory of specified type (if it has been registered previously)
	*
	* @param PostProcessType - PostProcess type
	*
	* @return - PostProcess factory pointer or nullptr if not registered
	*/
	virtual TSharedPtr<IDisplayClusterPostProcessFactory> GetPostProcessFactory(const FString& PostProcessType) = 0;

	/**
	* Returns all registered postprocess types
	*
	* @param OutPostProcessIDs - (out) array to put registered IDs
	*/
	virtual void GetRegisteredPostProcess(TArray<FString>& OutPostProcessIDs) const = 0;

	/**
	* Registers a post process operation
	*
	* @param Name      - A unique PP operation name
	* @param Operation - PP operation implementation
	* @param Priority  - PP order in chain (the calling order is from the smallest to the largest: -N...0...N)
	*
	* @return - True if success
	*/
	UE_DEPRECATED(4.27, "This function has been moved to FDisplayClusterViewport. Use GetViewportManager() to access  that interface.")
	virtual bool RegisterPostprocessOperation(const FString& Name, TSharedPtr<IDisplayClusterPostProcess, ESPMode::ThreadSafe>& Operation, int Priority = 0)
	{ return false; }

	/**
	* Registers a post process operation
	*
	* @param Name - A unique PP operation name
	* @param PPInfo - PP info wrapper (see IDisplayClusterRenderManager::FDisplayClusterPPInfo)
	*
	* @return - True if success
	*/
	UE_DEPRECATED(4.27, "This function has been moved to FDisplayClusterViewport. Use GetViewportManager() to access  that interface.")
	virtual bool RegisterPostprocessOperation(const FString& Name, FDisplayClusterPPInfo& PPInfo)
	{ return false; }

	/**
	* Unregisters a post process operation
	*
	* @param Name - PP operation name
	*
	* @return - True if success
	*/
	UE_DEPRECATED(4.27, "This function has been moved to FDisplayClusterViewport. Use GetViewportManager() to access  that interface.")
	virtual bool UnregisterPostprocessOperation(const FString& Name)
	{ return false; }

	/**
	* Returns all registered post-process operations
	*
	* @return - PP operations
	*/
	UE_DEPRECATED(4.27, "This function has been moved to FDisplayClusterViewport. Use GetViewportManager() to access  that interface.")
	virtual TMap<FString, FDisplayClusterPPInfo> GetRegisteredPostprocessOperations() const
	{ return TMap<FString, FDisplayClusterPPInfo>(); }

	/**
	* @return - Current viewport manager from root actor
	*/
	virtual IDisplayClusterViewportManager* GetViewportManager() const = 0;

};
