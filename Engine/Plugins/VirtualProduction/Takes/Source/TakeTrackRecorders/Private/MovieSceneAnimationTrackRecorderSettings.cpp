// Copyright Epic Games, Inc. All Rights Reserved.

#include "TrackRecorders/MovieSceneAnimationTrackRecorderSettings.h"

#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectBaseUtility.h"

void UMovieSceneAnimationTrackRecorderEditorSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		SaveConfig();
	}
}
