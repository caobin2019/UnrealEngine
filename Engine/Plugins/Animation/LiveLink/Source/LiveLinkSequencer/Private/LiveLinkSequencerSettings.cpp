// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkSequencerSettings.h"

FName ULiveLinkSequencerSettings::GetCategoryName() const
{
	return TEXT("Plugins");
}

#if WITH_EDITOR
FText ULiveLinkSequencerSettings::GetSectionText() const
{
	return NSLOCTEXT("LiveLinkSequencerSettings", "LiveLinkSequencerSettingsSection", "Live Link Sequencer");
}

FName ULiveLinkSequencerSettings::GetSectionName() const
{
	return TEXT("Live Link Sequencer");
}

#endif
