// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if !defined(WITH_MLSDK) || WITH_MLSDK

#include "Lumin/CAPIShims/LuminAPI.h"

LUMIN_THIRD_PARTY_INCLUDES_START
#include <ml_camera_metadata.h>
#include <ml_camera_metadata_tags.h>
LUMIN_THIRD_PARTY_INCLUDES_END

namespace LUMIN_MLSDK_API
{

CREATE_FUNCTION_SHIM(ml_camera_metadata, MLResult, MLCameraMetadataGetColorCorrectionAvailableAberrationModes)
#define MLCameraMetadataGetColorCorrectionAvailableAberrationModes ::LUMIN_MLSDK_API::MLCameraMetadataGetColorCorrectionAvailableAberrationModesShim
CREATE_FUNCTION_SHIM(ml_camera_metadata, MLResult, MLCameraMetadataGetControlAEAvailableModes)
#define MLCameraMetadataGetControlAEAvailableModes ::LUMIN_MLSDK_API::MLCameraMetadataGetControlAEAvailableModesShim
CREATE_FUNCTION_SHIM(ml_camera_metadata, MLResult, MLCameraMetadataGetControlAECompensationRange)
#define MLCameraMetadataGetControlAECompensationRange ::LUMIN_MLSDK_API::MLCameraMetadataGetControlAECompensationRangeShim
CREATE_FUNCTION_SHIM(ml_camera_metadata, MLResult, MLCameraMetadataGetControlAECompensationStep)
#define MLCameraMetadataGetControlAECompensationStep ::LUMIN_MLSDK_API::MLCameraMetadataGetControlAECompensationStepShim
CREATE_FUNCTION_SHIM(ml_camera_metadata, MLResult, MLCameraMetadataGetControlAELockAvailable)
#define MLCameraMetadataGetControlAELockAvailable ::LUMIN_MLSDK_API::MLCameraMetadataGetControlAELockAvailableShim
CREATE_FUNCTION_SHIM(ml_camera_metadata, MLResult, MLCameraMetadataGetControlAWBAvailableModes)
#define MLCameraMetadataGetControlAWBAvailableModes ::LUMIN_MLSDK_API::MLCameraMetadataGetControlAWBAvailableModesShim
CREATE_FUNCTION_SHIM(ml_camera_metadata, MLResult, MLCameraMetadataGetControlAWBLockAvailable)
#define MLCameraMetadataGetControlAWBLockAvailable ::LUMIN_MLSDK_API::MLCameraMetadataGetControlAWBLockAvailableShim
CREATE_FUNCTION_SHIM(ml_camera_metadata, MLResult, MLCameraMetadataGetScalerProcessedSizes)
#define MLCameraMetadataGetScalerProcessedSizes ::LUMIN_MLSDK_API::MLCameraMetadataGetScalerProcessedSizesShim
CREATE_FUNCTION_SHIM(ml_camera_metadata, MLResult, MLCameraMetadataGetScalerAvailableMaxDigitalZoom)
#define MLCameraMetadataGetScalerAvailableMaxDigitalZoom ::LUMIN_MLSDK_API::MLCameraMetadataGetScalerAvailableMaxDigitalZoomShim
CREATE_FUNCTION_SHIM(ml_camera_metadata, MLResult, MLCameraMetadataGetScalerAvailableStreamConfigurations)
#define MLCameraMetadataGetScalerAvailableStreamConfigurations ::LUMIN_MLSDK_API::MLCameraMetadataGetScalerAvailableStreamConfigurationsShim
CREATE_FUNCTION_SHIM(ml_camera_metadata, MLResult, MLCameraMetadataGetSensorInfoActiveArraySize)
#define MLCameraMetadataGetSensorInfoActiveArraySize ::LUMIN_MLSDK_API::MLCameraMetadataGetSensorInfoActiveArraySizeShim
CREATE_FUNCTION_SHIM(ml_camera_metadata, MLResult, MLCameraMetadataGetSensorInfoSensitivityRange)
#define MLCameraMetadataGetSensorInfoSensitivityRange ::LUMIN_MLSDK_API::MLCameraMetadataGetSensorInfoSensitivityRangeShim
CREATE_FUNCTION_SHIM(ml_camera_metadata, MLResult, MLCameraMetadataGetSensorInfoExposureTimeRange)
#define MLCameraMetadataGetSensorInfoExposureTimeRange ::LUMIN_MLSDK_API::MLCameraMetadataGetSensorInfoExposureTimeRangeShim
CREATE_FUNCTION_SHIM(ml_camera_metadata, MLResult, MLCameraMetadataGetSensorOrientation)
#define MLCameraMetadataGetSensorOrientation ::LUMIN_MLSDK_API::MLCameraMetadataGetSensorOrientationShim
CREATE_FUNCTION_SHIM(ml_camera_metadata, MLResult, MLCameraMetadataSetSensorInfoExposureTimeRange)
#define MLCameraMetadataSetSensorInfoExposureTimeRange ::LUMIN_MLSDK_API::MLCameraMetadataSetSensorInfoExposureTimeRangeShim
CREATE_FUNCTION_SHIM(ml_camera_metadata, MLResult, MLCameraMetadataGetColorCorrectionModeRequestMetadata)
#define MLCameraMetadataGetColorCorrectionModeRequestMetadata ::LUMIN_MLSDK_API::MLCameraMetadataGetColorCorrectionModeRequestMetadataShim
CREATE_FUNCTION_SHIM(ml_camera_metadata, MLResult, MLCameraMetadataGetColorCorrectionTransformRequestMetadata)
#define MLCameraMetadataGetColorCorrectionTransformRequestMetadata ::LUMIN_MLSDK_API::MLCameraMetadataGetColorCorrectionTransformRequestMetadataShim
CREATE_FUNCTION_SHIM(ml_camera_metadata, MLResult, MLCameraMetadataGetColorCorrectionGainsRequestMetadata)
#define MLCameraMetadataGetColorCorrectionGainsRequestMetadata ::LUMIN_MLSDK_API::MLCameraMetadataGetColorCorrectionGainsRequestMetadataShim
CREATE_FUNCTION_SHIM(ml_camera_metadata, MLResult, MLCameraMetadataGetColorCorrectionAberrationModeRequestMetadata)
#define MLCameraMetadataGetColorCorrectionAberrationModeRequestMetadata ::LUMIN_MLSDK_API::MLCameraMetadataGetColorCorrectionAberrationModeRequestMetadataShim
CREATE_FUNCTION_SHIM(ml_camera_metadata, MLResult, MLCameraMetadataGetControlAEAntibandingModeRequestMetadata)
#define MLCameraMetadataGetControlAEAntibandingModeRequestMetadata ::LUMIN_MLSDK_API::MLCameraMetadataGetControlAEAntibandingModeRequestMetadataShim
CREATE_FUNCTION_SHIM(ml_camera_metadata, MLResult, MLCameraMetadataGetControlAEExposureCompensationRequestMetadata)
#define MLCameraMetadataGetControlAEExposureCompensationRequestMetadata ::LUMIN_MLSDK_API::MLCameraMetadataGetControlAEExposureCompensationRequestMetadataShim
CREATE_FUNCTION_SHIM(ml_camera_metadata, MLResult, MLCameraMetadataGetControlAELockRequestMetadata)
#define MLCameraMetadataGetControlAELockRequestMetadata ::LUMIN_MLSDK_API::MLCameraMetadataGetControlAELockRequestMetadataShim
CREATE_FUNCTION_SHIM(ml_camera_metadata, MLResult, MLCameraMetadataGetControlAEModeRequestMetadata)
#define MLCameraMetadataGetControlAEModeRequestMetadata ::LUMIN_MLSDK_API::MLCameraMetadataGetControlAEModeRequestMetadataShim
CREATE_FUNCTION_SHIM(ml_camera_metadata, MLResult, MLCameraMetadataGetControlAETargetFPSRangeRequestMetadata)
#define MLCameraMetadataGetControlAETargetFPSRangeRequestMetadata ::LUMIN_MLSDK_API::MLCameraMetadataGetControlAETargetFPSRangeRequestMetadataShim
CREATE_FUNCTION_SHIM(ml_camera_metadata, MLResult, MLCameraMetadataGetControlAWBLockRequestMetadata)
#define MLCameraMetadataGetControlAWBLockRequestMetadata ::LUMIN_MLSDK_API::MLCameraMetadataGetControlAWBLockRequestMetadataShim
CREATE_FUNCTION_SHIM(ml_camera_metadata, MLResult, MLCameraMetadataGetControlAWBModeRequestMetadata)
#define MLCameraMetadataGetControlAWBModeRequestMetadata ::LUMIN_MLSDK_API::MLCameraMetadataGetControlAWBModeRequestMetadataShim
CREATE_FUNCTION_SHIM(ml_camera_metadata, MLResult, MLCameraMetadataGetSensorExposureTimeRequestMetadata)
#define MLCameraMetadataGetSensorExposureTimeRequestMetadata ::LUMIN_MLSDK_API::MLCameraMetadataGetSensorExposureTimeRequestMetadataShim
CREATE_FUNCTION_SHIM(ml_camera_metadata, MLResult, MLCameraMetadataGetSensorSensitivityRequestMetadata)
#define MLCameraMetadataGetSensorSensitivityRequestMetadata ::LUMIN_MLSDK_API::MLCameraMetadataGetSensorSensitivityRequestMetadataShim
CREATE_FUNCTION_SHIM(ml_camera_metadata, MLResult, MLCameraMetadataGetScalerCropRegionRequestMetadata)
#define MLCameraMetadataGetScalerCropRegionRequestMetadata ::LUMIN_MLSDK_API::MLCameraMetadataGetScalerCropRegionRequestMetadataShim
CREATE_FUNCTION_SHIM(ml_camera_metadata, MLResult, MLCameraMetadataSetColorCorrectionMode)
#define MLCameraMetadataSetColorCorrectionMode ::LUMIN_MLSDK_API::MLCameraMetadataSetColorCorrectionModeShim
CREATE_FUNCTION_SHIM(ml_camera_metadata, MLResult, MLCameraMetadataSetColorCorrectionTransform)
#define MLCameraMetadataSetColorCorrectionTransform ::LUMIN_MLSDK_API::MLCameraMetadataSetColorCorrectionTransformShim
CREATE_FUNCTION_SHIM(ml_camera_metadata, MLResult, MLCameraMetadataSetColorCorrectionGains)
#define MLCameraMetadataSetColorCorrectionGains ::LUMIN_MLSDK_API::MLCameraMetadataSetColorCorrectionGainsShim
CREATE_FUNCTION_SHIM(ml_camera_metadata, MLResult, MLCameraMetadataSetColorCorrectionAberrationMode)
#define MLCameraMetadataSetColorCorrectionAberrationMode ::LUMIN_MLSDK_API::MLCameraMetadataSetColorCorrectionAberrationModeShim
CREATE_FUNCTION_SHIM(ml_camera_metadata, MLResult, MLCameraMetadataSetControlAEAntibandingMode)
#define MLCameraMetadataSetControlAEAntibandingMode ::LUMIN_MLSDK_API::MLCameraMetadataSetControlAEAntibandingModeShim
CREATE_FUNCTION_SHIM(ml_camera_metadata, MLResult, MLCameraMetadataSetControlAEExposureCompensation)
#define MLCameraMetadataSetControlAEExposureCompensation ::LUMIN_MLSDK_API::MLCameraMetadataSetControlAEExposureCompensationShim
CREATE_FUNCTION_SHIM(ml_camera_metadata, MLResult, MLCameraMetadataSetControlAELock)
#define MLCameraMetadataSetControlAELock ::LUMIN_MLSDK_API::MLCameraMetadataSetControlAELockShim
CREATE_FUNCTION_SHIM(ml_camera_metadata, MLResult, MLCameraMetadataSetControlAEMode)
#define MLCameraMetadataSetControlAEMode ::LUMIN_MLSDK_API::MLCameraMetadataSetControlAEModeShim
CREATE_FUNCTION_SHIM(ml_camera_metadata, MLResult, MLCameraMetadataSetControlAETargetFPSRange)
#define MLCameraMetadataSetControlAETargetFPSRange ::LUMIN_MLSDK_API::MLCameraMetadataSetControlAETargetFPSRangeShim
CREATE_FUNCTION_SHIM(ml_camera_metadata, MLResult, MLCameraMetadataSetControlAWBLock)
#define MLCameraMetadataSetControlAWBLock ::LUMIN_MLSDK_API::MLCameraMetadataSetControlAWBLockShim
CREATE_FUNCTION_SHIM(ml_camera_metadata, MLResult, MLCameraMetadataSetControlAWBMode)
#define MLCameraMetadataSetControlAWBMode ::LUMIN_MLSDK_API::MLCameraMetadataSetControlAWBModeShim
CREATE_FUNCTION_SHIM(ml_camera_metadata, MLResult, MLCameraMetadataSetSensorExposureTime)
#define MLCameraMetadataSetSensorExposureTime ::LUMIN_MLSDK_API::MLCameraMetadataSetSensorExposureTimeShim
CREATE_FUNCTION_SHIM(ml_camera_metadata, MLResult, MLCameraMetadataSetSensorSensitivity)
#define MLCameraMetadataSetSensorSensitivity ::LUMIN_MLSDK_API::MLCameraMetadataSetSensorSensitivityShim
CREATE_FUNCTION_SHIM(ml_camera_metadata, MLResult, MLCameraMetadataSetScalerCropRegion)
#define MLCameraMetadataSetScalerCropRegion ::LUMIN_MLSDK_API::MLCameraMetadataSetScalerCropRegionShim
CREATE_FUNCTION_SHIM(ml_camera_metadata, MLResult, MLCameraMetadataGetColorCorrectionModeResultMetadata)
#define MLCameraMetadataGetColorCorrectionModeResultMetadata ::LUMIN_MLSDK_API::MLCameraMetadataGetColorCorrectionModeResultMetadataShim
CREATE_FUNCTION_SHIM(ml_camera_metadata, MLResult, MLCameraMetadataGetColorCorrectionTransformResultMetadata)
#define MLCameraMetadataGetColorCorrectionTransformResultMetadata ::LUMIN_MLSDK_API::MLCameraMetadataGetColorCorrectionTransformResultMetadataShim
CREATE_FUNCTION_SHIM(ml_camera_metadata, MLResult, MLCameraMetadataGetColorCorrectionAberrationModeResultMetadata)
#define MLCameraMetadataGetColorCorrectionAberrationModeResultMetadata ::LUMIN_MLSDK_API::MLCameraMetadataGetColorCorrectionAberrationModeResultMetadataShim
CREATE_FUNCTION_SHIM(ml_camera_metadata, MLResult, MLCameraMetadataGetColorCorrectionGainsResultMetadata)
#define MLCameraMetadataGetColorCorrectionGainsResultMetadata ::LUMIN_MLSDK_API::MLCameraMetadataGetColorCorrectionGainsResultMetadataShim
CREATE_FUNCTION_SHIM(ml_camera_metadata, MLResult, MLCameraMetadataGetControlAEAntibandingModeResultMetadata)
#define MLCameraMetadataGetControlAEAntibandingModeResultMetadata ::LUMIN_MLSDK_API::MLCameraMetadataGetControlAEAntibandingModeResultMetadataShim
CREATE_FUNCTION_SHIM(ml_camera_metadata, MLResult, MLCameraMetadataGetControlAEExposureCompensationResultMetadata)
#define MLCameraMetadataGetControlAEExposureCompensationResultMetadata ::LUMIN_MLSDK_API::MLCameraMetadataGetControlAEExposureCompensationResultMetadataShim
CREATE_FUNCTION_SHIM(ml_camera_metadata, MLResult, MLCameraMetadataGetControlAELockResultMetadata)
#define MLCameraMetadataGetControlAELockResultMetadata ::LUMIN_MLSDK_API::MLCameraMetadataGetControlAELockResultMetadataShim
CREATE_FUNCTION_SHIM(ml_camera_metadata, MLResult, MLCameraMetadataGetControlAEModeResultMetadata)
#define MLCameraMetadataGetControlAEModeResultMetadata ::LUMIN_MLSDK_API::MLCameraMetadataGetControlAEModeResultMetadataShim
CREATE_FUNCTION_SHIM(ml_camera_metadata, MLResult, MLCameraMetadataGetControlAETargetFPSRangeResultMetadata)
#define MLCameraMetadataGetControlAETargetFPSRangeResultMetadata ::LUMIN_MLSDK_API::MLCameraMetadataGetControlAETargetFPSRangeResultMetadataShim
CREATE_FUNCTION_SHIM(ml_camera_metadata, MLResult, MLCameraMetadataGetControlAEStateResultMetadata)
#define MLCameraMetadataGetControlAEStateResultMetadata ::LUMIN_MLSDK_API::MLCameraMetadataGetControlAEStateResultMetadataShim
CREATE_FUNCTION_SHIM(ml_camera_metadata, MLResult, MLCameraMetadataGetControlAWBLockResultMetadata)
#define MLCameraMetadataGetControlAWBLockResultMetadata ::LUMIN_MLSDK_API::MLCameraMetadataGetControlAWBLockResultMetadataShim
CREATE_FUNCTION_SHIM(ml_camera_metadata, MLResult, MLCameraMetadataGetControlAWBStateResultMetadata)
#define MLCameraMetadataGetControlAWBStateResultMetadata ::LUMIN_MLSDK_API::MLCameraMetadataGetControlAWBStateResultMetadataShim
CREATE_FUNCTION_SHIM(ml_camera_metadata, MLResult, MLCameraMetadataGetSensorExposureTimeResultMetadata)
#define MLCameraMetadataGetSensorExposureTimeResultMetadata ::LUMIN_MLSDK_API::MLCameraMetadataGetSensorExposureTimeResultMetadataShim
CREATE_FUNCTION_SHIM(ml_camera_metadata, MLResult, MLCameraMetadataGetSensorSensitivityResultMetadata)
#define MLCameraMetadataGetSensorSensitivityResultMetadata ::LUMIN_MLSDK_API::MLCameraMetadataGetSensorSensitivityResultMetadataShim
CREATE_FUNCTION_SHIM(ml_camera_metadata, MLResult, MLCameraMetadataGetSensorTimestampResultMetadata)
#define MLCameraMetadataGetSensorTimestampResultMetadata ::LUMIN_MLSDK_API::MLCameraMetadataGetSensorTimestampResultMetadataShim
CREATE_FUNCTION_SHIM(ml_camera_metadata, MLResult, MLCameraMetadataGetScalerCropRegionResultMetadata)
#define MLCameraMetadataGetScalerCropRegionResultMetadata ::LUMIN_MLSDK_API::MLCameraMetadataGetScalerCropRegionResultMetadataShim
CREATE_FUNCTION_SHIM(ml_camera_metadata, MLResult, MLCameraMetadataGetSensorFrameDurationResultMetadata)
#define MLCameraMetadataGetSensorFrameDurationResultMetadata ::LUMIN_MLSDK_API::MLCameraMetadataGetSensorFrameDurationResultMetadataShim

}

#endif // !defined(WITH_MLSDK) || WITH_MLSDK
