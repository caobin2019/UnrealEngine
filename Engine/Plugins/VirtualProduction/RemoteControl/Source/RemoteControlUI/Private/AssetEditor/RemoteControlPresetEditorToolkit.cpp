// Copyright Epic Games, Inc. All Rights Reserved.

#include "RemoteControlPresetEditorToolkit.h"

#include "Framework/Docking/TabManager.h"
#include "RemoteControlPreset.h"
#include "RemoteControlUIModule.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SWidget.h"
#include "UI/SRCPanelExposedEntitiesList.h"
#include "UI/SRCPanelTreeNode.h"
#include "UI/SRemoteControlPanel.h"

#define LOCTEXT_NAMESPACE "RemoteControlPresetEditorToolkit"

const FName FRemoteControlPresetEditorToolkit::PanelTabId(TEXT("RemoteControl_RemoteControlPanel"));
const FName FRemoteControlPresetEditorToolkit::RemoteControlPanelAppIdentifier(TEXT("RemoteControlPanel"));

TSharedRef<FRemoteControlPresetEditorToolkit> FRemoteControlPresetEditorToolkit::CreateEditor(const EToolkitMode::Type Mode, const TSharedPtr<class IToolkitHost>& InitToolkitHost, URemoteControlPreset* InPreset)
{
	TSharedRef<FRemoteControlPresetEditorToolkit> NewEditor = MakeShared<FRemoteControlPresetEditorToolkit>();

	NewEditor->InitRemoteControlPresetEditor(Mode, InitToolkitHost, InPreset);

	return NewEditor;
}

void FRemoteControlPresetEditorToolkit::InitRemoteControlPresetEditor(const EToolkitMode::Type Mode, const TSharedPtr<class IToolkitHost> & InitToolkitHost, URemoteControlPreset* InPreset)
{
	Preset = InPreset;

	PanelTab = FRemoteControlUIModule::Get().CreateRemoteControlPanel(InPreset);

	const TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout("Standalone_RemoteControlPresetEditor")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->Split
			(
				FTabManager::NewStack()
				->AddTab(PanelTabId, ETabState::OpenedTab)
			)
		);

	constexpr bool bCreateDefaultStandaloneMenu = true;
	constexpr bool bCreateDefaultToolbar = true;

	// Required, will cause the previous toolkit to close bringing down the RemoteControlPreset and unsubscribing the
	// tab spawner. Without this, the InitAssetEditor call below will trigger an ensure as the RemoteControlPreset
	// tab ID will already be registered within EditorTabManager
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	TSharedPtr<FTabManager> EditorTabManager = LevelEditorModule.GetLevelEditorTabManager();
	if (EditorTabManager->FindExistingLiveTab(PanelTabId).IsValid())
	{
		EditorTabManager->TryInvokeTab(PanelTabId)->RequestCloseTab();
	}
	
	FAssetEditorToolkit::InitAssetEditor(Mode, InitToolkitHost, RemoteControlPanelAppIdentifier, StandaloneDefaultLayout, bCreateDefaultStandaloneMenu, bCreateDefaultStandaloneMenu, InPreset);

	InvokePanelTab();
}

FRemoteControlPresetEditorToolkit::~FRemoteControlPresetEditorToolkit()
{
	if (FModuleManager::Get().IsModuleLoaded("LevelEditor"))
	{
		FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
		if (TSharedPtr<FTabManager> EditorTabManager = LevelEditorModule.GetLevelEditorTabManager())
		{
			UnregisterTabSpawners(EditorTabManager.ToSharedRef());
			if (TSharedPtr<SDockTab> Tab = EditorTabManager->FindExistingLiveTab(PanelTabId))
			{
				Tab->RequestCloseTab();
			}
		}
	}
}

void FRemoteControlPresetEditorToolkit::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_RemoteControlPanel", "Remote Control Panel"));

	InTabManager->RegisterTabSpawner(PanelTabId, FOnSpawnTab::CreateSP(this, &FRemoteControlPresetEditorToolkit::HandleTabManagerSpawnPanelTab))
		.SetDisplayName(LOCTEXT("RemoteControlPanelMainTab", "Remote Control Panel"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.GameSettings.Small"));
}

void FRemoteControlPresetEditorToolkit::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	InTabManager->UnregisterTabSpawner(PanelTabId);
}

bool FRemoteControlPresetEditorToolkit::OnRequestClose()
{
	return true;
}

void FRemoteControlPresetEditorToolkit::FocusWindow(UObject* ObjectToFocusOn)
{
	InvokePanelTab();
	BringToolkitToFront();
}

FText FRemoteControlPresetEditorToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("PanelToolkitName", "Remote Control Panel");
}

FName FRemoteControlPresetEditorToolkit::GetToolkitFName() const
{
	static const FName RemoteControlPanelName("RemoteControlPanel");
	return RemoteControlPanelName;
}

FLinearColor FRemoteControlPresetEditorToolkit::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.7, 0.0f, 0.0f, 0.5f);
}

FString FRemoteControlPresetEditorToolkit::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("RemoteControlTabPrefix", "RemoteControl ").ToString();
}

TSharedRef<SDockTab> FRemoteControlPresetEditorToolkit::HandleTabManagerSpawnPanelTab(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == PanelTabId);

	return SNew(SDockTab)
		.Label(LOCTEXT("ControlPanelLabel", "Control Panel"))
		.TabColorScale(GetTabColorScale())	
		[
			PanelTab.ToSharedRef()
		];
}

void FRemoteControlPresetEditorToolkit::InvokePanelTab()
{
	struct Local
	{
		static void OnRemoteControlPresetClosed(TSharedRef<SDockTab> DockTab, TWeakPtr<IAssetEditorInstance> InRemoteControlPreset)
		{
			TSharedPtr<IAssetEditorInstance> AssetEditorInstance = InRemoteControlPreset.Pin();

			if (AssetEditorInstance.IsValid())
			{
				InRemoteControlPreset.Pin()->CloseWindow();
			}
		}
	};

	// Create a new DockTab and add the RemoteControlPreset widget to it.
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	TSharedPtr<FTabManager> EditorTabManager = LevelEditorModule.GetLevelEditorTabManager();

	if (TSharedPtr<SDockTab> Tab = EditorTabManager->TryInvokeTab(PanelTabId))
	{
		Tab->SetContent(PanelTab.ToSharedRef());
		Tab->SetOnTabClosed(SDockTab::FOnTabClosedCallback::CreateStatic(&Local::OnRemoteControlPresetClosed, TWeakPtr<IAssetEditorInstance>(SharedThis(this))));
	}
}

#undef LOCTEXT_NAMESPACE /*RemoteControlPresetEditorToolkit*/