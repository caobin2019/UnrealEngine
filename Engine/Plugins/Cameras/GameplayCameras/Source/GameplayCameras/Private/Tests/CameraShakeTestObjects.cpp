// Copyright Epic Games, Inc. All Rights Reserved.

#include "CameraShakeTestObjects.h"

UConstantCameraShakePattern::UConstantCameraShakePattern(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UConstantCameraShakePattern::UpdateShakePatternImpl(const FCameraShakeUpdateParams& Params, FCameraShakeUpdateResult& OutResult)
{
	OutResult.Location = LocationOffset;
	OutResult.Rotation = RotationOffset;
}

