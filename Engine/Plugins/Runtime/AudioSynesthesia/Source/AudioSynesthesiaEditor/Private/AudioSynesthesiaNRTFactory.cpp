// Copyright Epic Games, Inc. All Rights Reserved.

#include "AudioSynesthesiaNRTFactory.h"

#include "ClassViewerModule.h"
#include "ClassViewerFilter.h"
#include "Kismet2/SClassPickerDialog.h"
#include "Modules/ModuleManager.h"
#include "AudioSynesthesiaClassFilter.h"

#define LOCTEXT_NAMESPACE "AudioSynesthesiaEditor"

UAudioSynesthesiaNRTFactory::UAudioSynesthesiaNRTFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UAudioSynesthesiaNRT::StaticClass();

	bCreateNew = true;
	bEditorImport = false;
	bEditAfterNew = true;
	AudioSynesthesiaNRTClass = nullptr;
}

bool UAudioSynesthesiaNRTFactory::ConfigureProperties()
{
	AudioSynesthesiaNRTClass = nullptr;

	// Load the classviewer module to display a class picker
	FClassViewerModule& ClassViewerModule = FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer");

	FClassViewerInitializationOptions Options;
	Options.Mode = EClassViewerMode::ClassPicker;

	TSharedPtr<Audio::FAssetClassParentFilter> Filter = MakeShareable(new Audio::FAssetClassParentFilter);
	Options.ClassFilter = Filter;

	Filter->DisallowedClassFlags = CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists;
	Filter->AllowedChildrenOfClasses.Add(UAudioSynesthesiaNRT::StaticClass());

	const FText TitleText = LOCTEXT("CreateAudioSynesthesiaNRTOptions", "Pick Synesthesia Class");
	UClass* ChosenClass = nullptr;
	const bool bPressedOk = SClassPickerDialog::PickClass(TitleText, Options, ChosenClass, UAudioSynesthesiaNRT::StaticClass());

	if (bPressedOk)
	{
		AudioSynesthesiaNRTClass = ChosenClass;
	}

	return bPressedOk;
}

UObject* UAudioSynesthesiaNRTFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UAudioSynesthesiaNRT* NewAudioSynesthesiaNRT = nullptr;
	if (AudioSynesthesiaNRTClass != nullptr)
	{
		NewAudioSynesthesiaNRT = NewObject<UAudioSynesthesiaNRT>(InParent, AudioSynesthesiaNRTClass, InName, Flags);
	}
	return NewAudioSynesthesiaNRT;
}

#undef LOCTEXT_NAMESPACE
