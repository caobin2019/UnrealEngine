// Copyright Epic Games, Inc. All Rights Reserved.

#include "SNiagaraDebugger.h"
#include "Editor/EditorStyle/Private/SlateEditorStyle.h"
#include "NiagaraEditorStyle.h"

#include "Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructure.h"
#include "Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructureModule.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SNumericDropDown.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/Docking/LayoutService.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "UObject/UObjectIterator.h"
#include "EditorWidgetsModule.h"
#include "PropertyEditorModule.h"
#include "ISessionServicesModule.h"
#include "Widgets/Browser/SSessionBrowser.h"
#include "Modules/ModuleManager.h"
#include "NiagaraEditorModule.h"
#include "Customizations/NiagaraDebugHUDCustomization.h"
#include "Customizations/NiagaraOutlinerCustomization.h"
#include "IStructureDetailsView.h"


#if WITH_NIAGARA_DEBUGGER

#define LOCTEXT_NAMESPACE "SNiagaraDebugger"

const FName SNiagaraDebugger::DebugWindowName(TEXT("NiagaraDebugger"));

namespace NiagaraDebuggerLocal
{
	DECLARE_DELEGATE_TwoParams(FOnExecConsoleCommand, const TCHAR*, bool);

	template<typename T>
	static TAttribute<T> CreateTAttribute(TFunction<T()> InFunction)
	{
		return TAttribute<T>::Create(TAttribute<T>::FGetter::CreateLambda(InFunction));
	}
}

namespace NiagaraDebugHudTab
{
	static const FName TabName = FName(TEXT("DebugHudTab"));

	static void RegisterTabSpawner(const TSharedPtr<FTabManager>& TabManager)
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

		UNiagaraDebugHUDSettings* DebugHudSettings = GetMutableDefault<UNiagaraDebugHUDSettings>();

		FDetailsViewArgs DetailsViewArgs;
		DetailsViewArgs.bHideSelectionTip = true;
		DetailsViewArgs.NotifyHook = DebugHudSettings;

		FStructureDetailsViewArgs StructureViewArgs;
		StructureViewArgs.bShowObjects = true;
		StructureViewArgs.bShowAssets = true;
		StructureViewArgs.bShowClasses = true;
		StructureViewArgs.bShowInterfaces = true;

		TSharedRef<IStructureDetailsView> StructureDetailsView = PropertyModule.CreateStructureDetailView(DetailsViewArgs, StructureViewArgs, nullptr);
		StructureDetailsView->GetDetailsView()->SetGenericLayoutDetailsDelegate(FOnGetDetailCustomizationInstance::CreateStatic(&FNiagaraDebugHUDSettingsDetailsCustomization::MakeInstance, DebugHudSettings));

		TSharedPtr<FStructOnScope> StructOnScope = MakeShared<FStructOnScope>(FNiagaraDebugHUDSettingsData::StaticStruct(), reinterpret_cast<uint8*>(&DebugHudSettings->Data));
		StructureDetailsView->SetStructureData(StructOnScope);

		TabManager->RegisterTabSpawner(
			TabName,
			FOnSpawnTab::CreateLambda(
				[=](const FSpawnTabArgs&)
				{
					return SNew(SDockTab)
						.TabRole(ETabRole::PanelTab)
						.Label(LOCTEXT("DebugHudTitle", "Debug Hud"))
						[
							StructureDetailsView->GetWidget().ToSharedRef()
						];
				}
			)
		)
		.SetDisplayName(LOCTEXT("DebugHudTabTitle", "Debug Hud"))
		.SetTooltipText(LOCTEXT("DebugHudTooltipText", "Open the Debug Hud tab."));
	}
}

namespace NiagaraPerformanceTab
{
	static const FName TabName = FName(TEXT("PerformanceTab"));

	class SPerformanceWidget : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SPerformanceWidget) {}
			SLATE_ARGUMENT(NiagaraDebuggerLocal::FOnExecConsoleCommand, ExecConsoleCommand)
		SLATE_END_ARGS();

		void Construct(const FArguments& InArgs)
		{
			using namespace NiagaraDebuggerLocal;

			ExecConsoleCommand = InArgs._ExecConsoleCommand;

			ChildSlot
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SUniformGridPanel)
					.SlotPadding(FEditorStyle::GetMargin("StandardDialog.SlotPadding"))
					.MinDesiredSlotWidth(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
					.MinDesiredSlotHeight(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
					+ SUniformGridPanel::Slot(0, 0)
					[
						SNew(SButton)
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Center)
						.OnClicked(FOnClicked::CreateLambda([&]() { ExecConsoleCommand.ExecuteIfBound(TEXT("stat particleperf"), true); return FReply::Handled(); }))
						.Text(LOCTEXT("ToggleParticlePerf", "Toggle ParticlePerf"))
						.ToolTipText(LOCTEXT("ToggleParticlePerfTooltip", "Toggles particle performance stat display on & off"))
					]
					+ SUniformGridPanel::Slot(1, 0)
					[
						SNew(SButton)
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Center)
						.OnClicked(FOnClicked::CreateLambda([&]() { ExecConsoleCommand.ExecuteIfBound(*FString::Printf(TEXT("fx.ParticlePerfStats.RunTest %d"), PerfTestNumFrames), true); return FReply::Handled(); } ))
						.ToolTipText(LOCTEXT("RunPerfTestTooltip", "Runs performance tests for the number of frames and dumps results to the log / csv."))
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(0.0f, 0.0f, 6.0f, 0.0f)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("RunPerfTest", "Run Performance Test"))
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							[
								SNew(SNumericEntryBox<int>)
								.Value(CreateTAttribute<TOptional<int>>([&]() { return TOptional<int>(PerfTestNumFrames); }))
								.AllowSpin(true)
								.MinValue(1)
								.MaxValue(TOptional<int>())
								.MinSliderValue(1)
								.MaxSliderValue(60*10)
								.OnValueChanged(SNumericEntryBox<int>::FOnValueChanged::CreateLambda([&](int InNewValue) { PerfTestNumFrames = InNewValue; }))
							]
						]
					]
					+ SUniformGridPanel::Slot(0, 1)
					[
						SNew(SButton)
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Center)
						.OnClicked(FOnClicked::CreateLambda([&]() { ExecConsoleCommand.ExecuteIfBound(TEXT("stat NiagaraBaselines"), true); return FReply::Handled(); }))
						.Text(LOCTEXT("ToggleBaseline", "Toggle Baseline"))
						.ToolTipText(LOCTEXT("ToggleBaselineTooltip", "Toggles baseline performance display on & off."))
					]
					+ SUniformGridPanel::Slot(0, 2)
					[
						SNew(SButton)
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Center)
						.OnClicked(FOnClicked::CreateLambda(
							[&]()
							{
								ExecConsoleCommand.ExecuteIfBound(TEXT("fx.ParallelSystemInstanceTick 1"), true);
								ExecConsoleCommand.ExecuteIfBound(TEXT("fx.ParallelSystemSimTick 1"), true);
								return FReply::Handled();
							}
						))
						.Text(LOCTEXT("EnableAsyncSim", "Enable Async Simulation"))
						.ToolTipText(LOCTEXT("EnableAsyncSimTooltip", "Overrides existing settings to enable async simulations."))
					]
					+ SUniformGridPanel::Slot(1, 2)
					[
						SNew(SButton)
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Center)
						.OnClicked(FOnClicked::CreateLambda(
							[&]()
							{
								ExecConsoleCommand.ExecuteIfBound(TEXT("fx.ParallelSystemInstanceTick 0"), true);
								ExecConsoleCommand.ExecuteIfBound(TEXT("fx.ParallelSystemSimTick 0"), true);
								return FReply::Handled();
							}
						))
						.Text(LOCTEXT("DisableAsyncSim", "Disable Async Simulation"))
						.ToolTipText(LOCTEXT("DisableAsyncSimTooltip", "Overrides existing settings to disable async simulations."))
					]
				]
			];
		}
	
	private:
		NiagaraDebuggerLocal::FOnExecConsoleCommand ExecConsoleCommand;
		int	PerfTestNumFrames = 60;
	};

	static void RegisterTabSpawner(const TSharedPtr<FTabManager>& TabManager, NiagaraDebuggerLocal::FOnExecConsoleCommand ExecConsoleCommand)
	{
		TabManager->RegisterTabSpawner(
			TabName,
			FOnSpawnTab::CreateLambda(
				[=](const FSpawnTabArgs&)
				{
					return SNew(SDockTab)
						.TabRole(ETabRole::PanelTab)
						.Label(LOCTEXT("PerformanceTitle", "Performance"))
						[
							SNew(SPerformanceWidget)
							.ExecConsoleCommand(ExecConsoleCommand)
						];
				}
			)
		)
		.SetDisplayName(LOCTEXT("PerformanceTabTitle", "Performance"))
		.SetTooltipText(LOCTEXT("PerformanceTooltipText", "Open the Performance tab."));
	}
}

namespace NiagaraOutlinerTab
{
	static const FName TabName = FName(TEXT("OutlinerTab"));

	static TSharedRef<SWidget> MakeOutlinerToolbar(TSharedPtr<FNiagaraDebugger>& Debugger)
	{
		if (!Debugger.IsValid())
		{
			return SNullWidget::NullWidget;
		}
		UNiagaraOutliner* Outliner = Debugger->GetOutliner();
		if (!ensure(Outliner))
		{
			return SNullWidget::NullWidget;
		}

		FToolBarBuilder ToolbarBuilder(MakeShareable(new FUICommandList), FMultiBoxCustomization::None);
		UNiagaraDebugHUDSettings* Settings = GetMutableDefault<UNiagaraDebugHUDSettings>();
		ToolbarBuilder.BeginSection("Capture Settings");

		// Capture controls
		{
			// Capture button
			{
				ToolbarBuilder.AddToolBarButton(
					FUIAction(FExecuteAction::CreateLambda([Debugger]() 
					{
						if (UNiagaraOutliner* Outliner = Debugger->GetOutliner())
						{
							Outliner->CaptureSettings.bTriggerCapture = true;
							Outliner->OnChanged();
						}
					})),
					NAME_None,
					LOCTEXT("Capture", "Capture"),
					LOCTEXT("CaptureTooltip", "Triggers a capture of outliner info from the connected session."),
					FSlateIcon(FNiagaraEditorStyle::GetStyleSetName(), "NiagaraEditor.Debugger.Outliner.Capture"),
					EUserInterfaceActionType::Button
				);
			}

			// Gather Perf Toggle
			{
				ToolbarBuilder.AddToolBarButton(
					FUIAction(
						FExecuteAction::CreateLambda([Debugger]() 
						{
							if (UNiagaraOutliner* Outliner = Debugger->GetOutliner())
							{
								Outliner->CaptureSettings.bGatherPerfData ^= 1;
								Outliner->OnChanged();
							}
						}),
						FCanExecuteAction(),
						FIsActionChecked::CreateLambda([Debugger]() { return Debugger->GetOutliner()->CaptureSettings.bGatherPerfData; })
					),
					NAME_None,
					LOCTEXT("GatherOutlinerPerfData", "Perf"),
					LOCTEXT("GatherOutlinerPerfDataTooltip", "Gather Performance data during outliner capture."),
					FSlateIcon(FNiagaraEditorStyle::GetStyleSetName(), "NiagaraEditor.Debugger.Outliner.CapturePerf"),
					EUserInterfaceActionType::ToggleButton
				);
			}

 			// Capture delay
 			{
				TSharedRef<SWidget> DelayWidget = SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("NoBorder"))
				.Padding(FMargin(3.0, 0.0f, 3.0f, 0.0f))
				.ToolTipText(LOCTEXT("OutlinerDelayTooltip", "Number of frames to delay between a capture being triggered and it being taken.\nThis provides time to affect the scene and also defines the length of time performance data is gathered."))				
				[
					SNew(SEditableTextBox)
					.OnTextCommitted(FOnTextCommitted::CreateLambda([Debugger](const FText& InText, ETextCommit::Type CommitInfo)
					{
						if (UNiagaraOutliner* Outliner = Debugger->GetOutliner())
						{
							LexFromString(Outliner->CaptureSettings.CaptureDelayFrames, *InText.ToString());
							Outliner->OnChanged();
						}
					}))
					.Text(MakeAttributeLambda([Debugger]()
					{
						if (UNiagaraOutliner* Outliner = Debugger->GetOutliner())
						{
							return FText::AsNumber(Outliner->CaptureSettings.CaptureDelayFrames);
						}
						return FText::GetEmpty();
					}))					
				];
				ToolbarBuilder.AddToolBarWidget(DelayWidget, LOCTEXT("OutlinerDelay", "Delay"));
			}
		}

		ToolbarBuilder.AddSeparator();

		//View Settings
		{
			// View Mode
			{
				auto GetViewModeText = [Debugger]()
				{
					UEnum* Enum = StaticEnum<ENiagaraOutlinerViewModes>();
					check(Enum);
					if (UNiagaraOutliner* Outliner = Debugger->GetOutliner())
					{
						ENiagaraOutlinerViewModes ViewMode = Outliner->ViewSettings.ViewMode;
						return Enum->GetDisplayNameTextByValue((int32)ViewMode);
					}
					return FText::GetEmpty();
				};
				auto GetViewModeMenu = [Debugger]()
				{
					FMenuBuilder MenuBuilder(true, NULL);
					UEnum* Enum = StaticEnum<ENiagaraOutlinerViewModes>();
					check(Enum);
					if (UNiagaraOutliner* Outliner = Debugger->GetOutliner())
					{
						for (int32 i = 0; i < Enum->NumEnums() - 1; ++i)
						{
							FUIAction ItemAction(FExecuteAction::CreateLambda([Debugger, NewMode=(ENiagaraOutlinerViewModes)Enum->GetValueByIndex(i)]
							{
								if (UNiagaraOutliner* Outliner = Debugger->GetOutliner())
								{
									Outliner->ViewSettings.ViewMode = NewMode;
									Outliner->OnChanged();
								}
							}));
							MenuBuilder.AddMenuEntry(Enum->GetDisplayNameTextByIndex(i), Enum->GetToolTipTextByIndex(i), FSlateIcon(), ItemAction);
						}
					}

					return MenuBuilder.MakeWidget();
				};

				TSharedRef<SWidget> ViewModeWidget = SNew(SComboButton)
					.OnGetMenuContent(FOnGetContent::CreateLambda(GetViewModeMenu))
					.ButtonStyle(FNiagaraEditorStyle::Get(), "NiagaraEditor.Debugger.Outliner.Toolbar")
					.ButtonContent()
					[
						SNew(STextBlock)
						.ToolTipText(LOCTEXT("ViewMode", "View Mode"))
						.Text(MakeAttributeLambda(GetViewModeText))
					];

				ToolbarBuilder.AddToolBarWidget(ViewModeWidget, LOCTEXT("OutlinerViewMode", "View Mode"));
			}

			ToolbarBuilder.AddSeparator();

			//Filters
			{
				FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

				TSharedPtr<FStructOnScope> FiltersData = MakeShared<FStructOnScope>(FNiagaraOutlinerFilterSettings::StaticStruct(), (uint8*)&Outliner->ViewSettings.FilterSettings);
		
				FDetailsViewArgs DetailsViewArgs(false, false, false, FDetailsViewArgs::HideNameArea, true, Outliner);
				DetailsViewArgs.bShowScrollBar = false;
				DetailsViewArgs.ColumnWidth = 0.4f;
				TSharedRef<IStructureDetailsView> FilterDetails = PropertyEditorModule.CreateStructureDetailView(
						DetailsViewArgs,
						FStructureDetailsViewArgs(),
						nullptr);
			
				FilterDetails->SetStructureData(FiltersData);

				auto OnFiltersChanged = [Debugger, FiltersData](const FPropertyChangedEvent& PropertyChangedEvent)
				{
					if (Debugger.IsValid() && FiltersData.IsValid())
					{
						Debugger->GetOutliner()->OnChanged();
					}
				};
				FilterDetails->GetOnFinishedChangingPropertiesDelegate().AddLambda(OnFiltersChanged);				

				TSharedRef<SMenuAnchor> FiltersMenu = SNew(SMenuAnchor)
				.Method(EPopupMethod::UseCurrentWindow)
				.Placement(MenuPlacement_ComboBox)
				.OnGetMenuContent(FOnGetContent::CreateLambda([Debugger, FiltersData, FilterDetails]()
				{
					return SNew(SBorder)
						.BorderImage(FEditorStyle::GetBrush("Menu.Background"))
						.Padding(FMargin(2))
						[
							FilterDetails->GetWidget().ToSharedRef()
						];
				}));
				
				FiltersMenu->SetContent(
					SNew(SButton)				
					.ButtonStyle(FNiagaraEditorStyle::Get(), "NiagaraEditor.Debugger.Outliner.Toolbar")
					.ForegroundColor(FSlateColor::UseForeground())
					.OnClicked_Lambda([FiltersMenu]() -> FReply 
					{
						FiltersMenu->SetIsOpen(true);
						return FReply::Handled();
					})
					[
						SNew(SImage)
						.Image(FNiagaraEditorStyle::Get().GetBrush("NiagaraEditor.Debugger.Outliner.Filter"))
					]
				);

				ToolbarBuilder.AddToolBarWidget(FiltersMenu, LOCTEXT("OutlinerFiltersLabel", "Filters"));
			}
			
			ToolbarBuilder.AddSeparator();

			// Sorting
			{
				//Sort Descending
				{
					ToolbarBuilder.AddToolBarButton(
					FUIAction(
						FExecuteAction::CreateLambda([Debugger]()
							{
								if (Debugger.IsValid())
								{
									Debugger->GetOutliner()->ViewSettings.bSortDescending ^= 1;
									Debugger->GetOutliner()->OnChanged();
								}
							}),
						FCanExecuteAction(),
								FIsActionChecked::CreateLambda([Debugger]() 
								{
									if(Debugger.IsValid())
									{
										return Debugger->GetOutliner()->ViewSettings.bSortDescending; 
									}
									return false;
								})
								),
								NAME_None,
								LOCTEXT("SortDecsending", "Descending"),
								LOCTEXT("SortDecsendingTooltip", "Sort Descending or Ascending"),
								FSlateIcon(FSlateEditorStyle::GetStyleSetName(), "Profiler.Misc.SortDescending"),
								EUserInterfaceActionType::ToggleButton
								);
				}

				//Sort Mode
				{
					auto GetSortModeText = [Debugger]()
					{
						UEnum* Enum = StaticEnum<ENiagaraOutlinerSortMode>();
						if (Debugger.IsValid() && Enum)
						{
							ENiagaraOutlinerSortMode SortMode = Debugger->GetOutliner()->ViewSettings.SortMode;
							return Enum->GetDisplayNameTextByValue((int32)SortMode);
						}
						return FText::GetEmpty();
					};
					auto GetSortModeMenu = [Debugger]()
					{
						FMenuBuilder MenuBuilder(true, NULL);
						UEnum* Enum = StaticEnum<ENiagaraOutlinerSortMode>();
						if (Debugger.IsValid() && Enum)
						{
							for (int32 i = 0; i < Enum->NumEnums() - 1; ++i)
							{
								FUIAction ItemAction(FExecuteAction::CreateLambda([Debugger, NewMode = (ENiagaraOutlinerSortMode)Enum->GetValueByIndex(i)]
								{
									if (Debugger.IsValid())
									{
										Debugger->GetOutliner()->ViewSettings.SortMode = NewMode;
										Debugger->GetOutliner()->OnChanged();
									}
								}));
								MenuBuilder.AddMenuEntry(Enum->GetDisplayNameTextByIndex(i), Enum->GetToolTipTextByIndex(i), FSlateIcon(), ItemAction);
							}
						}

						return MenuBuilder.MakeWidget();
					};

					TSharedRef<SWidget> SortModeWidget = SNew(SComboButton)
						.OnGetMenuContent(FOnGetContent::CreateLambda(GetSortModeMenu))
						.ButtonStyle(FNiagaraEditorStyle::Get(), "NiagaraEditor.Debugger.Outliner.Toolbar")
						.ButtonContent()
						[
							SNew(STextBlock)
							.ToolTipText(LOCTEXT("SortMode", "Sort Mode"))
							.Text(MakeAttributeLambda(GetSortModeText))
						];

					ToolbarBuilder.AddToolBarWidget(SortModeWidget, LOCTEXT("OutlinerSortMode", "Sort Mode"));
				}
			}

			ToolbarBuilder.AddSeparator();

			//Time units
			{
				auto GetUnitsText = [Debugger]()
				{
					UEnum* Enum = StaticEnum<ENiagaraOutlinerTimeUnits>();
					if (Debugger.IsValid() && Enum)
					{
						ENiagaraOutlinerTimeUnits TimeUnits = Debugger->GetOutliner()->ViewSettings.TimeUnits;
						return Enum->GetDisplayNameTextByValue((int32)TimeUnits);
					}
					return FText::GetEmpty();
				};
				auto GetUnitsMenu = [Debugger]()
				{
					FMenuBuilder MenuBuilder(true, NULL);
					UEnum* Enum = StaticEnum<ENiagaraOutlinerTimeUnits>();
					if (Debugger.IsValid() && Enum)
					{
						for (int32 i = 0; i < Enum->NumEnums() - 1; ++i)
						{
							FUIAction ItemAction(FExecuteAction::CreateLambda([Debugger, NewMode=(ENiagaraOutlinerTimeUnits)Enum->GetValueByIndex(i)]
							{
								if (Debugger.IsValid())
								{
									Debugger->GetOutliner()->ViewSettings.TimeUnits = NewMode;
									Debugger->GetOutliner()->OnChanged();
								}
							}));
							MenuBuilder.AddMenuEntry(Enum->GetDisplayNameTextByIndex(i), Enum->GetToolTipTextByIndex(i), FSlateIcon(), ItemAction);
						}
					}

					return MenuBuilder.MakeWidget();
				};

				TSharedRef<SWidget> UnitsWidget = SNew(SComboButton)
					.OnGetMenuContent(FOnGetContent::CreateLambda(GetUnitsMenu))
					.ButtonStyle(FNiagaraEditorStyle::Get(), "NiagaraEditor.Debugger.Outliner.Toolbar")
					.ButtonContent()
					[
						SNew(STextBlock)
						.ToolTipText(LOCTEXT("TimeUnits", "Units"))
						.Text(MakeAttributeLambda(GetUnitsText))
					];

				ToolbarBuilder.AddToolBarWidget(UnitsWidget, LOCTEXT("OutlinerTimeUnits", "Units"));
			}
		}

		ToolbarBuilder.EndSection();

		return ToolbarBuilder.MakeWidget();
	}

	static void RegisterTabSpawner(const TSharedPtr<FTabManager>& TabManager, TSharedPtr<FNiagaraDebugger>& Debugger)
	{
		TSharedRef<SWidget> OutlinerToolbar = MakeOutlinerToolbar(Debugger);

		TabManager->RegisterTabSpawner(
			TabName,
			FOnSpawnTab::CreateLambda(
				[=](const FSpawnTabArgs&)
				{
					return SNew(SDockTab)
						.TabRole(ETabRole::PanelTab)
						.Label(LOCTEXT("OutlinerTitle", "FX Outliner"))
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot()
							.AutoHeight()
							[
								OutlinerToolbar
							]
 							+ SVerticalBox::Slot()
							.Padding(2.0)
							[
								SNew(SNiagaraOutlinerTree, Debugger)
							]
						];
				}
			)
		)
		.SetDisplayName(LOCTEXT("OutlinerTabTitle", "FX Outliner"))
		.SetTooltipText(LOCTEXT("OutlinerTooltipText", "Open the FX Outliner tab."));
	}
}

namespace NiagaraSessionBrowserTab
{
	static const FName TabName = FName(TEXT("Session Browser"));

	static void RegisterTabSpawner(const TSharedPtr<FTabManager>& TabManager, TSharedPtr<ISessionManager>& SessionManager)
	{
		TabManager->RegisterTabSpawner(
			TabName,
			FOnSpawnTab::CreateLambda(
				[=](const FSpawnTabArgs&)
				{
					return SNew(SDockTab)
						.TabRole(ETabRole::PanelTab)
						.Label(LOCTEXT("SessionBrowser", "Session Browser"))
						[
							SNew(SSessionBrowser, SessionManager.ToSharedRef())
						];
				}
			)
		)
			.SetDisplayName(LOCTEXT("SessionBrowserTabTitle", "Session Browser"))
					.SetTooltipText(LOCTEXT("SessionBrowserTooltipText", "Open the Session Browser tab."));
	}
}


SNiagaraDebugger::SNiagaraDebugger()
{
}

SNiagaraDebugger::~SNiagaraDebugger()
{
}

void SNiagaraDebugger::Construct(const FArguments& InArgs)
{
	using namespace NiagaraDebuggerLocal;

	TabManager = InArgs._TabManager;
	Debugger = InArgs._Debugger;

	NiagaraDebugHudTab::RegisterTabSpawner(TabManager);
	NiagaraPerformanceTab::RegisterTabSpawner(TabManager, FOnExecConsoleCommand::CreateSP(Debugger.ToSharedRef(), &FNiagaraDebugger::ExecConsoleCommand));
	NiagaraOutlinerTab::RegisterTabSpawner(TabManager, Debugger);

	ISessionServicesModule& SessionServicesModule = FModuleManager::LoadModuleChecked<ISessionServicesModule>("SessionServices");
	TSharedPtr<ISessionManager> SessionManager = SessionServicesModule.GetSessionManager();
	NiagaraSessionBrowserTab::RegisterTabSpawner(TabManager, SessionManager);

	TSharedPtr<FTabManager::FLayout> DebuggerLayout = FTabManager::NewLayout("NiagaraDebugger_Layout_v1.11")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Horizontal)
			->Split
			(
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Vertical)
				->SetSizeCoefficient(0.3f)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(.8f)
					->SetHideTabWell(true)
					->AddTab(NiagaraDebugHudTab::TabName, ETabState::OpenedTab)
					->AddTab(NiagaraOutlinerTab::TabName, ETabState::OpenedTab)
					->AddTab(NiagaraPerformanceTab::TabName, ETabState::OpenedTab)
					->AddTab(NiagaraSessionBrowserTab::TabName, ETabState::OpenedTab)
					->SetForegroundTab(NiagaraDebugHudTab::TabName)
				)
			)
		);

	DebuggerLayout = FLayoutSaveRestore::LoadFromConfig(GEditorLayoutIni, DebuggerLayout.ToSharedRef());

	TSharedRef<SWidget> TabContents = TabManager->RestoreFrom(DebuggerLayout.ToSharedRef(), TSharedPtr<SWindow>()).ToSharedRef();

	// create & initialize main menu
	FMenuBarBuilder MenuBarBuilder = FMenuBarBuilder(TSharedPtr<FUICommandList>());

	MenuBarBuilder.AddPullDownMenu(
		LOCTEXT("WindowMenuLabel", "Window"),
		FText::GetEmpty(),
		FNewMenuDelegate::CreateSP(this, &SNiagaraDebugger::FillWindowMenu),
		"Window"
	);

	// Tell tab-manager about the multi-box for platforms with a global menu bar
	TabManager->SetMenuMultiBox(MenuBarBuilder.GetMultiBox());

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			MenuBarBuilder.MakeWidget()
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			MakeToolbar()
		]
		+ SVerticalBox::Slot()
		.Padding(2.0)
		[
			TabContents
		]
	];
}

void SNiagaraDebugger::FillWindowMenu(FMenuBuilder& MenuBuilder)
{
	if (!TabManager.IsValid())
	{
		return;
	}

#if !WITH_EDITOR
	FGlobalTabmanager::Get()->PopulateTabSpawnerMenu(MenuBuilder, WorkspaceMenu::GetMenuStructure().GetStructureRoot());
#endif //!WITH_EDITOR

	TabManager->PopulateLocalTabSpawnerMenu(MenuBuilder);
}

void SNiagaraDebugger::RegisterTabSpawner()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		SNiagaraDebugger::DebugWindowName,
		FOnSpawnTab::CreateStatic(&SNiagaraDebugger::SpawnNiagaraDebugger)
	)
	.SetDisplayName(NSLOCTEXT("UnrealEditor", "NiagaraDebuggerTab", "Niagara Debugger"))
	.SetTooltipText(NSLOCTEXT("UnrealEditor", "NiagaraDebuggerTooltipText", "Open the Niagara Debugger Tab."))
	.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsDebugCategory())
	.SetIcon(FSlateIcon(FNiagaraEditorStyle::GetStyleSetName(), "NiagaraEditor.Debugger.TabIcon"));
}

void SNiagaraDebugger::UnregisterTabSpawner()
{
	if (FSlateApplication::IsInitialized())
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(SNiagaraDebugger::DebugWindowName);
	}
}

TSharedRef<SDockTab> SNiagaraDebugger::SpawnNiagaraDebugger(const FSpawnTabArgs& Args)
{
	auto NomadTab = SNew(SDockTab)
		.Icon(FNiagaraEditorStyle::Get().GetBrush("NiagaraEditor.Debugger.TabIcon"))
		.TabRole(ETabRole::NomadTab)
		.Label(NSLOCTEXT("NiagaraDebugger", "NiagaraDebuggerTabTitle", "Niagara Debugger"));

	auto TabManager = FGlobalTabmanager::Get()->NewTabManager(NomadTab);
	TabManager->SetOnPersistLayout(
		FTabManager::FOnPersistLayout::CreateStatic(
			[](const TSharedRef<FTabManager::FLayout>& InLayout)
			{
				if (InLayout->GetPrimaryArea().Pin().IsValid())
				{
					FLayoutSaveRestore::SaveToConfig(GEditorLayoutIni, InLayout);
				}
			}
		)
	);

	NomadTab->SetOnTabClosed(
		SDockTab::FOnTabClosedCallback::CreateStatic(
			[](TSharedRef<SDockTab> Self, TWeakPtr<FTabManager> TabManager)
			{
				TSharedPtr<FTabManager> OwningTabManager = TabManager.Pin();
				if (OwningTabManager.IsValid())
				{
					FLayoutSaveRestore::SaveToConfig(GEditorLayoutIni, OwningTabManager->PersistLayout());
					OwningTabManager->CloseAllAreas();
				}
			}
			, TWeakPtr<FTabManager>(TabManager)
		)
	);

	FNiagaraEditorModule& NiagaraEditorModule = FModuleManager::GetModuleChecked<FNiagaraEditorModule>("NiagaraEditor");

	auto MainWidget = SNew(SNiagaraDebugger)
		.TabManager(TabManager)
		.Debugger(NiagaraEditorModule.GetDebugger());

	NomadTab->SetContent(MainWidget);
	return NomadTab;
}

void SNiagaraDebugger::FocusDebugTab()
{
	TabManager->TryInvokeTab(NiagaraDebugHudTab::TabName);
}

void SNiagaraDebugger::FocusOutlineTab()
{
	TabManager->TryInvokeTab(NiagaraOutlinerTab::TabName);
}

TSharedRef<SWidget> SNiagaraDebugger::MakeToolbar()
{
	using namespace NiagaraDebuggerLocal;

	FToolBarBuilder ToolbarBuilder(MakeShareable(new FUICommandList), FMultiBoxCustomization::None);
	UNiagaraDebugHUDSettings* Settings = GetMutableDefault<UNiagaraDebugHUDSettings>();
	ToolbarBuilder.BeginSection("Main");

	// Refresh button
	{
		ToolbarBuilder.AddToolBarButton(
			FUIAction(FExecuteAction::CreateLambda([Owner=Debugger]() { Owner->UpdateDebugHUDSettings(); })),
			NAME_None,
			LOCTEXT("Refresh", "Refresh"),
			LOCTEXT("RefreshTooltip", "Refesh the settings on the target device.  Used if we get out of sync."),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "Icons.Refresh")
		);
	}

	ToolbarBuilder.AddSeparator();

	// Playback controls
	{
		// Play Button
		{
			ToolbarBuilder.AddToolBarButton(
				FUIAction(
					FExecuteAction::CreateLambda([=]() {Settings->Data.PlaybackMode = ENiagaraDebugPlaybackMode::Play; Settings->NotifyPropertyChanged(); }),
					FCanExecuteAction(),
					FIsActionChecked::CreateLambda([=]() { return Settings->Data.PlaybackMode == ENiagaraDebugPlaybackMode::Play; })
				),
				NAME_None,
				LOCTEXT("Play", "Play"),
				LOCTEXT("PlayTooltip", "Simulations will play as normal"),
				FSlateIcon(FNiagaraEditorStyle::GetStyleSetName(), "NiagaraEditor.Debugger.PlayIcon"),
				EUserInterfaceActionType::ToggleButton
			);
		}
		// Pause Button
		{
			ToolbarBuilder.AddToolBarButton(
				FUIAction(
					FExecuteAction::CreateLambda([=]() {Settings->Data.PlaybackMode = ENiagaraDebugPlaybackMode::Paused; Settings->NotifyPropertyChanged(); }),
					FCanExecuteAction(),
					FIsActionChecked::CreateLambda([=]() { return Settings->Data.PlaybackMode == ENiagaraDebugPlaybackMode::Paused; })
				),
				NAME_None,
				LOCTEXT("Pause", "Pause"),
				LOCTEXT("PauseTooltip", "Pause all simulations"),
				FSlateIcon(FNiagaraEditorStyle::GetStyleSetName(), "NiagaraEditor.Debugger.PauseIcon"),
				EUserInterfaceActionType::ToggleButton
			);
		}
		// Loop Button
		{
			ToolbarBuilder.AddToolBarButton(
				FUIAction(
					FExecuteAction::CreateLambda([=]() {Settings->Data.PlaybackMode = ENiagaraDebugPlaybackMode::Loop; Settings->NotifyPropertyChanged(); }),
					FCanExecuteAction(),
					FIsActionChecked::CreateLambda([=]() { return Settings->Data.PlaybackMode == ENiagaraDebugPlaybackMode::Loop; })
				),
				NAME_None,
				CreateTAttribute<FText>([=]() { return Settings->Data.bLoopTimeEnabled ? FText::Format(LOCTEXT("PlaybackLoopFormat", "Loop Every\n{0} Seconds"), FText::AsNumber(Settings->Data.LoopTime)) : LOCTEXT("Loop", "Loop"); }),
				LOCTEXT("LoopTooltip", "Loop all simulations, i.e. one shot effects will loop"),
				FSlateIcon(FNiagaraEditorStyle::GetStyleSetName(), "NiagaraEditor.Debugger.LoopIcon"),
				EUserInterfaceActionType::ToggleButton
			);
		}
		// Step Button
		{
			ToolbarBuilder.AddToolBarButton(
				FUIAction(
					FExecuteAction::CreateLambda([=]() {Settings->Data.PlaybackMode = ENiagaraDebugPlaybackMode::Step; Settings->NotifyPropertyChanged(); Settings->Data.PlaybackMode = ENiagaraDebugPlaybackMode::Paused; }),
					FCanExecuteAction(),
					FIsActionChecked::CreateLambda([=]() { return Settings->Data.PlaybackMode == ENiagaraDebugPlaybackMode::Step; })
				),
				NAME_None,
				LOCTEXT("Step", "Step"),
				LOCTEXT("StepTooltip", "Step all simulations a single frame then pause them"),
				FSlateIcon(FNiagaraEditorStyle::GetStyleSetName(), "NiagaraEditor.Debugger.StepIcon"),
				EUserInterfaceActionType::ToggleButton
			);
		}
		// Speed Button
		{
			ToolbarBuilder.AddToolBarButton(
				FUIAction(
					FExecuteAction::CreateLambda([=]() {Settings->Data.bPlaybackRateEnabled = !Settings->Data.bPlaybackRateEnabled; Settings->NotifyPropertyChanged(); }),
					FCanExecuteAction(),
					FIsActionChecked::CreateLambda([=]() { return Settings->Data.bPlaybackRateEnabled; })
				),
				NAME_None,
				CreateTAttribute<FText>([=]() { return  FText::Format(LOCTEXT("PlaybackSpeedFormat", "Speed\n{0} x"), FText::AsNumber(Settings->Data.PlaybackRate)); }),
				LOCTEXT("SlowTooltip", "When enabled adjusts the playback speed for simulations."),
				FSlateIcon(FNiagaraEditorStyle::GetStyleSetName(), "NiagaraEditor.Debugger.SpeedIcon"),
				EUserInterfaceActionType::ToggleButton
			);
		}
		// Additional options
		ToolbarBuilder.AddComboButton(
			FUIAction(),
			FOnGetContent::CreateSP(this, &SNiagaraDebugger::MakePlaybackOptionsMenu),
			FText(),
			LOCTEXT("PlaybackOptionsTooltip", "Additional options to control playback."),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "MaterialEditor.ToggleMaterialStats"),
			true
		);
	}

	ToolbarBuilder.EndSection();

	return ToolbarBuilder.MakeWidget();
}

TSharedRef<SWidget> SNiagaraDebugger::MakePlaybackOptionsMenu()
{
	using namespace NiagaraDebuggerLocal;

	FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection=*/true, nullptr);
	MenuBuilder.BeginSection(NAME_None, LOCTEXT("PlaybackSpeed", "Playback Speed"));
	{
		static const TTuple<float, FText, FText> PlaybackSpeeds[] =
		{
			MakeTuple(1.0000f,	LOCTEXT("PlaybackSpeed_Normal", "Normal Speed"),		LOCTEXT("NormalSpeedTooltip", "Set playback speed to normal")),
			MakeTuple(0.5000f,	LOCTEXT("PlaybackSpeed_Half", "Half Speed"),			LOCTEXT("NormalSpeedTooltip", "Set playback speed to half the normal speed")),
			MakeTuple(0.2500f,	LOCTEXT("PlaybackSpeed_Quarter", "Quarter Speed "),		LOCTEXT("NormalSpeedTooltip", "Set playback speed to quarter the normal speed")),
			MakeTuple(0.1250f,	LOCTEXT("PlaybackSpeed_Eighth", "Eighth Speed "),		LOCTEXT("NormalSpeedTooltip", "Set playback speed to eighth the normal speed")),
			MakeTuple(0.0625f,	LOCTEXT("PlaybackSpeed_Sixteenth", "Sixteenth Speed "),	LOCTEXT("NormalSpeedTooltip", "Set playback speed to sixteenth the normal speed")),
		};
		UNiagaraDebugHUDSettings* Settings = GetMutableDefault<UNiagaraDebugHUDSettings>();

		for ( const auto& Speed : PlaybackSpeeds )
		{
			MenuBuilder.AddMenuEntry(
				Speed.Get<1>(),
				Speed.Get<2>(),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateLambda([Settings, Rate=Speed.Get<0>()]() { Settings->Data.PlaybackRate = Rate; Settings->NotifyPropertyChanged(); }),
					FCanExecuteAction(),
					FIsActionChecked::CreateLambda([Settings, Rate=Speed.Get<0>()]() { return FMath::IsNearlyEqual(Settings->Data.PlaybackRate, Rate); })
				),
				NAME_None,
				EUserInterfaceActionType::RadioButton
			);
		}

		MenuBuilder.AddWidget(
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 4.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("CustomSpeed", "Custom Speed"))
			]
			+ SHorizontalBox::Slot()
			[
				SNew(SNumericEntryBox<float>)
				.Value(CreateTAttribute<TOptional<float>>([=]() { return TOptional<float>(Settings->Data.PlaybackRate); }))
				.AllowSpin(true)
				.MinValue(0.0f)
				.MaxValue(TOptional<float>())
				.MinSliderValue(0.0f)
				.MaxSliderValue(1.0f)
				.OnValueChanged(SNumericEntryBox<float>::FOnValueChanged::CreateLambda([=](float InNewValue) { Settings->Data.PlaybackRate = InNewValue; Settings->NotifyPropertyChanged(); }))
			],
			FText()
		);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("LoopTime", "Loop Time"));
	{
		UNiagaraDebugHUDSettings* Settings = GetMutableDefault<UNiagaraDebugHUDSettings>();

		MenuBuilder.AddMenuEntry(
			LOCTEXT("LoopTimeEnabled", "Enabled"),
			LOCTEXT("LoopTimeEnabledTooltip", "When enabled and in loop mode systems will loop on this time rather than when they finish"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([Settings]() { Settings->Data.bLoopTimeEnabled = !Settings->Data.bLoopTimeEnabled; Settings->NotifyPropertyChanged(); }),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([Settings]() { return Settings->Data.bLoopTimeEnabled; })
			),
			NAME_None,
			EUserInterfaceActionType::Check
		);

		MenuBuilder.AddWidget(
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 4.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("LoopTime", "Loop Time"))
			]
			+ SHorizontalBox::Slot()
			[
				SNew(SNumericEntryBox<float>)
				.Value(CreateTAttribute<TOptional<float>>([=]() { return TOptional<float>(Settings->Data.LoopTime); }))
				.AllowSpin(true)
				.MinValue(0.0f)
				.MaxValue(TOptional<float>())
				.MinSliderValue(0.0f)
				.MaxSliderValue(5.0f)
				.OnValueChanged(SNumericEntryBox<float>::FOnValueChanged::CreateLambda([=](float InNewValue) { Settings->Data.LoopTime = InNewValue; Settings->NotifyPropertyChanged(); }))
			],
			FText()
		);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}
#undef LOCTEXT_NAMESPACE

#endif
