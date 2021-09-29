// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if !defined(WITH_MLSDK) || WITH_MLSDK

#include "Lumin/CAPIShims/LuminAPI.h"

LUMIN_THIRD_PARTY_INCLUDES_START
#include <ml_found_object.h>
LUMIN_THIRD_PARTY_INCLUDES_END

namespace LUMIN_MLSDK_API
{

CREATE_FUNCTION_SHIM(ml_perception_client, MLResult, MLFoundObjectTrackerCreate)
#define MLFoundObjectTrackerCreate ::LUMIN_MLSDK_API::MLFoundObjectTrackerCreateShim
CREATE_FUNCTION_SHIM(ml_perception_client, MLResult, MLFoundObjectTrackerInitSettings)
#define MLFoundObjectTrackerInitSettings ::LUMIN_MLSDK_API::MLFoundObjectTrackerInitSettingsShim
CREATE_FUNCTION_SHIM(ml_perception_client, MLResult, MLFoundObjectTrackerUpdateSettings)
#define MLFoundObjectTrackerUpdateSettings ::LUMIN_MLSDK_API::MLFoundObjectTrackerUpdateSettingsShim
CREATE_FUNCTION_SHIM(ml_perception_client, MLResult, MLFoundObjectQuery)
#define MLFoundObjectQuery ::LUMIN_MLSDK_API::MLFoundObjectQueryShim
CREATE_FUNCTION_SHIM(ml_perception_client, MLResult, MLFoundObjectGetResultCount)
#define MLFoundObjectGetResultCount ::LUMIN_MLSDK_API::MLFoundObjectGetResultCountShim
CREATE_FUNCTION_SHIM(ml_perception_client, MLResult, MLFoundObjectGetResult)
#define MLFoundObjectGetResult ::LUMIN_MLSDK_API::MLFoundObjectGetResultShim
CREATE_FUNCTION_SHIM(ml_perception_client, MLResult, MLFoundObjectGetProperty)
#define MLFoundObjectGetProperty ::LUMIN_MLSDK_API::MLFoundObjectGetPropertyShim
CREATE_FUNCTION_SHIM(ml_perception_client, MLResult, MLFoundObjectRequestPropertyData)
#define MLFoundObjectRequestPropertyData ::LUMIN_MLSDK_API::MLFoundObjectRequestPropertyDataShim
CREATE_FUNCTION_SHIM(ml_perception_client, MLResult, MLFoundObjectGetPropertyData)
#define MLFoundObjectGetPropertyData ::LUMIN_MLSDK_API::MLFoundObjectGetPropertyDataShim
CREATE_FUNCTION_SHIM(ml_perception_client, MLResult, MLFoundObjectSetBinding)
#define MLFoundObjectSetBinding ::LUMIN_MLSDK_API::MLFoundObjectSetBindingShim
CREATE_FUNCTION_SHIM(ml_perception_client, MLResult, MLFoundObjectRemoveBinding)
#define MLFoundObjectRemoveBinding ::LUMIN_MLSDK_API::MLFoundObjectRemoveBindingShim
CREATE_FUNCTION_SHIM(ml_perception_client, MLResult, MLFoundObjectGetBindingCount)
#define MLFoundObjectGetBindingCount ::LUMIN_MLSDK_API::MLFoundObjectGetBindingCountShim
CREATE_FUNCTION_SHIM(ml_perception_client, MLResult, MLFoundObjectGetFoundObjectBindingCount)
#define MLFoundObjectGetFoundObjectBindingCount ::LUMIN_MLSDK_API::MLFoundObjectGetFoundObjectBindingCountShim
CREATE_FUNCTION_SHIM(ml_perception_client, MLResult, MLFoundObjectGetBinding)
#define MLFoundObjectGetBinding ::LUMIN_MLSDK_API::MLFoundObjectGetBindingShim
CREATE_FUNCTION_SHIM(ml_perception_client, MLResult, MLFoundObjectGetBindingByVirtualId)
#define MLFoundObjectGetBindingByVirtualId ::LUMIN_MLSDK_API::MLFoundObjectGetBindingByVirtualIdShim
CREATE_FUNCTION_SHIM(ml_perception_client, MLResult, MLFoundObjectGetAvailableLabelsCount)
#define MLFoundObjectGetAvailableLabelsCount ::LUMIN_MLSDK_API::MLFoundObjectGetAvailableLabelsCountShim
CREATE_FUNCTION_SHIM(ml_perception_client, MLResult, MLFoundObjectGetUniqueLabel)
#define MLFoundObjectGetUniqueLabel ::LUMIN_MLSDK_API::MLFoundObjectGetUniqueLabelShim
CREATE_FUNCTION_SHIM(ml_perception_client, MLResult, MLFoundObjectSubscribe)
#define MLFoundObjectSubscribe ::LUMIN_MLSDK_API::MLFoundObjectSubscribeShim
CREATE_FUNCTION_SHIM(ml_perception_client, MLResult, MLFoundObjectUnsubscribe)
#define MLFoundObjectUnsubscribe ::LUMIN_MLSDK_API::MLFoundObjectUnsubscribeShim
CREATE_FUNCTION_SHIM(ml_perception_client, MLResult, MLFoundObjectGetSubscriptionList)
#define MLFoundObjectGetSubscriptionList ::LUMIN_MLSDK_API::MLFoundObjectGetSubscriptionListShim
CREATE_FUNCTION_SHIM(ml_perception_client, MLResult, MLFoundObjectGetSubscriptionIdAtIndex)
#define MLFoundObjectGetSubscriptionIdAtIndex ::LUMIN_MLSDK_API::MLFoundObjectGetSubscriptionIdAtIndexShim
CREATE_FUNCTION_SHIM(ml_perception_client, MLResult, MLFoundObjectGetStateChangesCount)
#define MLFoundObjectGetStateChangesCount ::LUMIN_MLSDK_API::MLFoundObjectGetStateChangesCountShim
CREATE_FUNCTION_SHIM(ml_perception_client, MLResult, MLFoundObjectGetStateChangesCountForIndex)
#define MLFoundObjectGetStateChangesCountForIndex ::LUMIN_MLSDK_API::MLFoundObjectGetStateChangesCountForIndexShim
CREATE_FUNCTION_SHIM(ml_perception_client, MLResult, MLFoundObjectGetNextStateChange)
#define MLFoundObjectGetNextStateChange ::LUMIN_MLSDK_API::MLFoundObjectGetNextStateChangeShim
CREATE_FUNCTION_SHIM(ml_perception_client, MLResult, MLFoundObjectTrackerDestroy)
#define MLFoundObjectTrackerDestroy ::LUMIN_MLSDK_API::MLFoundObjectTrackerDestroyShim

}

#endif // !defined(WITH_MLSDK) || WITH_MLSDK
