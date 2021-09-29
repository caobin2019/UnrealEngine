// Copyright Epic Games, Inc. All Rights Reserved.

#include "SNiagaraParameterMenu.h"

#include "AssetRegistryModule.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "NiagaraActions.h"
#include "NiagaraConstants.h"
#include "NiagaraEditorSettings.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraGraph.h"
#include "NiagaraNode.h"
#include "NiagaraNodeAssignment.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraNodeParameterMapBase.h"
#include "NiagaraNodeWithDynamicPins.h"
#include "NiagaraParameterDefinitions.h"
#include "NiagaraScriptVariable.h"
#include "NiagaraScriptSource.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemEditorData.h"
#include "SGraphActionMenu.h"
#include "SNiagaraGraphActionWidget.h"
#include "ViewModels/NiagaraSystemViewModel.h"
#include "ViewModels/TNiagaraViewModelManager.h"
#include "Widgets/SNiagaraActionMenuExpander.h"

#define LOCTEXT_NAMESPACE "SNiagaraParameterMenu"

///////////////////////////////////////////////////////////////////////////////
/// Base Parameter Menu														///
///////////////////////////////////////////////////////////////////////////////

void SNiagaraParameterMenu::Construct(const FArguments& InArgs)
{
	this->bAutoExpandMenu = InArgs._AutoExpandMenu;

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("Menu.Background"))
		.Padding(5)
		[
			SNew(SBox)
			.MinDesiredWidth(300)
			.MaxDesiredHeight(700) // Set max desired height to prevent flickering bug for menu larger than screen
			[
			SAssignNew(GraphMenu, SGraphActionMenu)
			.OnActionSelected(this, &SNiagaraParameterMenu::OnActionSelected)
			.OnCollectAllActions(this, &SNiagaraParameterMenu::CollectAllActions)
			.SortItemsRecursively(false)
			.AlphaSortItems(false)
			.AutoExpandActionMenu(bAutoExpandMenu)
			.ShowFilterTextBox(true)
			.OnGetSectionTitle(InArgs._OnGetSectionTitle)
			.OnCreateCustomRowExpander_Static(&SNiagaraParameterMenu::CreateCustomActionExpander)
			.OnCreateWidgetForAction_Lambda([](const FCreateWidgetForActionData* InData)
				{
					return SNew(SNiagaraGraphActionWidget, InData);
				})
			]
		]
	];
}

TSharedPtr<SEditableTextBox> SNiagaraParameterMenu::GetSearchBox()
{
	return GraphMenu->GetFilterTextBox();
}

void SNiagaraParameterMenu::OnActionSelected(const TArray<TSharedPtr<FEdGraphSchemaAction>>& SelectedActions, ESelectInfo::Type InSelectionType)
{
	if (InSelectionType == ESelectInfo::OnMouseClick || InSelectionType == ESelectInfo::OnKeyPress || SelectedActions.Num() == 0)
	{
		for (int32 ActionIndex = 0; ActionIndex < SelectedActions.Num(); ActionIndex++)
		{
			TSharedPtr<FNiagaraMenuAction> CurrentAction = StaticCastSharedPtr<FNiagaraMenuAction>(SelectedActions[ActionIndex]);

			if (CurrentAction.IsValid())
			{
				FSlateApplication::Get().DismissAllMenus();
				CurrentAction->ExecuteAction();
			}
		}
	}
}

TSharedRef<SExpanderArrow> SNiagaraParameterMenu::CreateCustomActionExpander(const struct FCustomExpanderData& ActionMenuData)
{
	return SNew(SNiagaraActionMenuExpander, ActionMenuData);
}

bool SNiagaraParameterMenu::IsStaticSwitchParameter(const FNiagaraVariable& Variable, const TArray<UNiagaraGraph*>& Graphs)
{
	for (const UNiagaraGraph* Graph : Graphs)
	{
		TArray<FNiagaraVariable> SwitchInputs = Graph->FindStaticSwitchInputs();
		if (SwitchInputs.Contains(Variable))
		{
			return true;
		}
	}
	return false;
}

FText SNiagaraParameterMenu::GetNamespaceCategoryText(const FNiagaraNamespaceMetadata& NamespaceMetaData)
{
	return NamespaceMetaData.DisplayNameLong.IsEmptyOrWhitespace() == false ? NamespaceMetaData.DisplayNameLong : NamespaceMetaData.DisplayName;
}

FText SNiagaraParameterMenu::GetNamespaceCategoryText(const FGuid& NamespaceId)
{
	const FNiagaraNamespaceMetadata NamespaceMetaData = FNiagaraEditorUtilities::GetNamespaceMetaDataForId(NamespaceId);
	return GetNamespaceCategoryText(NamespaceMetaData);
}


///////////////////////////////////////////////////////////////////////////////
/// Add Parameter Menu														///
///////////////////////////////////////////////////////////////////////////////

void SNiagaraAddParameterFromPanelMenu::Construct(const FArguments& InArgs)
{
	this->OnAddParameter = InArgs._OnAddParameter;
	this->OnAddScriptVar = InArgs._OnAddScriptVar;
	this->OnAllowMakeType = InArgs._OnAllowMakeType;
	this->OnAddParameterDefinitions = InArgs._OnAddParameterDefinitions;

	this->Graphs = InArgs._Graphs;
	this->AvailableParameterDefinitions = InArgs._AvailableParameterDefinitions;
	this->SubscribedParameterDefinitions = InArgs._SubscribedParameterDefinitions;
	this->NamespaceId = InArgs._NamespaceId;
	this->bAllowCreatingNew = InArgs._AllowCreatingNew;
	this->bShowNamespaceCategory = InArgs._ShowNamespaceCategory;
	this->bShowGraphParameters = InArgs._ShowGraphParameters;
	this->bIsParameterReadNode = InArgs._IsParameterRead;
	this->bForceCollectEngineNamespaceParameterActions = InArgs._ForceCollectEngineNamespaceParameterActions;
	this->bCullParameterActionsAlreadyInGraph = InArgs._CullParameterActionsAlreadyInGraph;
	this->AdditionalCulledParameterNames = InArgs._AdditionalCulledParameterNames;
	this->AssignmentNode = InArgs._AssignmentNode;
	this->bOnlyShowParametersInNamespaceId = NamespaceId.IsValid();

	SNiagaraParameterMenu::FArguments SuperArgs;
	SuperArgs._AutoExpandMenu = InArgs._AutoExpandMenu;
	SuperArgs._OnGetSectionTitle = FGetSectionTitle::CreateStatic(&SNiagaraAddParameterFromPanelMenu::GetSectionTitle);
	SNiagaraParameterMenu::Construct(SuperArgs);
}

void SNiagaraAddParameterFromPanelMenu::CollectMakeNew(FNiagaraMenuActionCollector& Collector, const FGuid& InNamespaceId)
{
	if (bAllowCreatingNew == false)
	{
		return;
	}

	TArray<FNiagaraVariable> Variables;
	TConstArrayView<FNiagaraTypeDefinition> SectionTypes;
	if(InNamespaceId == FNiagaraEditorGuids::UserNamespaceMetaDataGuid) 
	{
		SectionTypes = MakeArrayView(FNiagaraTypeRegistry::GetRegisteredUserVariableTypes());
	}
	else if(InNamespaceId == FNiagaraEditorGuids::SystemNamespaceMetaDataGuid)
	{
		SectionTypes = MakeArrayView(FNiagaraTypeRegistry::GetRegisteredSystemVariableTypes());
	}
	else if (InNamespaceId == FNiagaraEditorGuids::EmitterNamespaceMetaDataGuid)
	{
		SectionTypes = MakeArrayView(FNiagaraTypeRegistry::GetRegisteredEmitterVariableTypes());
	}
	else if (InNamespaceId == FNiagaraEditorGuids::ParticleAttributeNamespaceMetaDataGuid)
	{
		SectionTypes = MakeArrayView(FNiagaraTypeRegistry::GetRegisteredParticleVariableTypes());
	}
	else
	{
		SectionTypes = MakeArrayView(FNiagaraTypeRegistry::GetRegisteredTypes());
	}

	for (const FNiagaraTypeDefinition& RegisteredType : SectionTypes)
	{
		bool bAllowType = true;
		if (OnAllowMakeType.IsBound())
		{
			bAllowType = OnAllowMakeType.Execute(RegisteredType);
		}

		if (bAllowType)
		{
			FNiagaraVariable Var(RegisteredType, FName(*RegisteredType.GetNameText().ToString()));
			FNiagaraEditorUtilities::ResetVariableToDefaultValue(Var);
			Variables.Add(Var);
		}
	}

	AddParameterGroup(Collector, Variables, InNamespaceId,
		LOCTEXT("MakeNewCat", "Make New"), 1,
		bShowNamespaceCategory ? GetNamespaceCategoryText(InNamespaceId).ToString() : FString());
}

void SNiagaraAddParameterFromPanelMenu::AddParameterGroup(
	FNiagaraMenuActionCollector& Collector,
	TArray<FNiagaraVariable>& Variables,
	const FGuid& InNamespaceId /*= FGuid()*/,
	const FText& Category /*= FText::GetEmpty()*/,
	int32 SortOrder /*= 0*/,
	const FString& RootCategory /*= FString()*/,
	const bool bCreateUniqueName /*= true*/)
{
	for (const FNiagaraVariable& Variable : Variables)
	{
		const FText DisplayName = FText::FromName(Variable.GetName());
		FText Tooltip = FText::GetEmpty();

		if (const UStruct* VariableStruct = Variable.GetType().GetStruct())
		{
			Tooltip = VariableStruct->GetToolTipText(true);
		}
		if (const FNiagaraVariableMetaData* VariableMetaData = FNiagaraConstants::GetConstantMetaData(Variable))
		{
			FText Text = VariableMetaData->Description;
			if (Text.IsEmptyOrWhitespace() == false)
			{
				Tooltip = Text;
			}
		}

		FText SubCategory = FNiagaraEditorUtilities::GetVariableTypeCategory(Variable);
		FText FullCategory = SubCategory.IsEmpty() ? Category : FText::Format(FText::FromString("{0}|{1}"), Category, SubCategory);
		TSharedPtr<FNiagaraMenuAction> Action(new FNiagaraMenuAction(FullCategory, DisplayName, Tooltip, 0, FText(),
			FNiagaraMenuAction::FOnExecuteStackAction::CreateSP(this, &SNiagaraAddParameterFromPanelMenu::ParameterSelected, Variable, bCreateUniqueName, InNamespaceId)));
		Action->SetParameterVariable(Variable);

		if (Variable.IsDataInterface())
		{
			if (const UClass* DataInterfaceClass = Variable.GetType().GetClass())
			{
				Action->IsExperimental = DataInterfaceClass->GetMetaData("DevelopmentStatus") == TEXT("Experimental");
			}
		}

		Collector.AddAction(Action, SortOrder, RootCategory);
	}
}

void SNiagaraAddParameterFromPanelMenu::CollectParameterCollectionsActions(FNiagaraMenuActionCollector& Collector)
{
	//Create sub menus for parameter collections.
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<FAssetData> CollectionAssets;
	AssetRegistryModule.Get().GetAssetsByClass(UNiagaraParameterCollection::StaticClass()->GetFName(), CollectionAssets);

	const FText Category = GetNamespaceCategoryText(FNiagaraEditorGuids::ParameterCollectionNamespaceMetaDataGuid);
	for (FAssetData& CollectionAsset : CollectionAssets)
	{
		UNiagaraParameterCollection* Collection = CastChecked<UNiagaraParameterCollection>(CollectionAsset.GetAsset());
		if (Collection)
		{
			AddParameterGroup(Collector, Collection->GetParameters(), FNiagaraEditorGuids::ParameterCollectionNamespaceMetaDataGuid, Category, 10, FString(), false); //TODO
		}
	}
}

void SNiagaraAddParameterFromPanelMenu::CollectAllActions(FGraphActionListBuilderBase& OutAllActions)
{
	FNiagaraMenuActionCollector Collector;
	auto CollectEngineNamespaceParameterActions = [this, &Collector]() {
		const FNiagaraNamespaceMetadata NamespaceMetaData = FNiagaraEditorUtilities::GetNamespaceMetaDataForId(FNiagaraEditorGuids::EngineNamespaceMetaDataGuid);
		TArray<FNiagaraVariable> Variables = FNiagaraConstants::GetEngineConstants();
		const FText CategoryText = bShowNamespaceCategory ? GetNamespaceCategoryText(NamespaceMetaData) : LOCTEXT("EngineConstantNamespaceCategory", "Add Engine Constant");
		const FString RootCategoryStr = FString();
		const bool bMakeNameUnique = false;
		AddParameterGroup(
			Collector,
			Variables,
			FNiagaraEditorGuids::EngineNamespaceMetaDataGuid,
			CategoryText,
			4,
			RootCategoryStr,
			bMakeNameUnique
		);
	};
	auto CollectEmitterNamespaceParameterActions = [this, &Collector]() {
		const FNiagaraNamespaceMetadata NamespaceMetaData = FNiagaraEditorUtilities::GetNamespaceMetaDataForId(FNiagaraEditorGuids::EmitterNamespaceMetaDataGuid);
		TArray<FNiagaraVariable> Variables = FNiagaraConstants::GetEngineConstants().FilterByPredicate([](const FNiagaraVariable& Var) { return Var.IsInNameSpace(FNiagaraConstants::EmitterNamespace); });
		const FText CategoryText = bShowNamespaceCategory ? GetNamespaceCategoryText(NamespaceMetaData) : LOCTEXT("EngineConstantNamespaceCategory", "Add Emitter Constant");
		const FString RootCategoryStr = FString();
		const bool bMakeNameUnique = false;
		AddParameterGroup(
			Collector,
			Variables,
			FNiagaraEditorGuids::EngineNamespaceMetaDataGuid,
			CategoryText,
			4,
			RootCategoryStr,
			bMakeNameUnique
		);
	};

	TSet<FGuid> ExistingGraphParameterIds;
	TSet<FName> VisitedParameterNames;
	// Append additional culled parameter names to visited parameter names so that we preemptively cull any parameters that are name matching.
	VisitedParameterNames.Append(AdditionalCulledParameterNames);
	TArray<FGuid> ExcludedNamespaceIds;
	// If this is a write node, exclude any read-only vars.
	if (!bIsParameterReadNode)
	{
		ExcludedNamespaceIds.Add(FNiagaraEditorGuids::UserNamespaceMetaDataGuid);
		ExcludedNamespaceIds.Add(FNiagaraEditorGuids::EngineNamespaceMetaDataGuid);
		ExcludedNamespaceIds.Add(FNiagaraEditorGuids::ParameterCollectionNamespaceMetaDataGuid);
	}

	// If this doesn't have particles in the script, exclude reading or writing them.
	// Also, collect the ids of all variables the graph owns to exclude them from parameters to add from libraries.
	for (const UNiagaraGraph* Graph : Graphs)
	{
		bool IsModule = Graph->FindOutputNode(ENiagaraScriptUsage::Module) != nullptr || Graph->FindOutputNode(ENiagaraScriptUsage::DynamicInput) != nullptr
			|| Graph->FindOutputNode(ENiagaraScriptUsage::Function) != nullptr;

		UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(Graph->GetOuter());
		if (Source && IsModule)
		{
			UNiagaraScript* Script = Cast<UNiagaraScript>(Source->GetOuter());
			if (Script)
			{
				TArray<ENiagaraScriptUsage> Usages = Script->GetLatestScriptData()->GetSupportedUsageContexts();
				if (!Usages.Contains(ENiagaraScriptUsage::ParticleEventScript) &&
					!Usages.Contains(ENiagaraScriptUsage::ParticleSpawnScript) &&
					!Usages.Contains(ENiagaraScriptUsage::ParticleUpdateScript))
				{
					ExcludedNamespaceIds.Add(FNiagaraEditorGuids::ParticleAttributeNamespaceMetaDataGuid);
				}

				if (bIsParameterReadNode)
				{
					if (!Usages.Contains(ENiagaraScriptUsage::SystemSpawnScript) &&
						!Usages.Contains(ENiagaraScriptUsage::SystemUpdateScript))
					{
						ExcludedNamespaceIds.Add(FNiagaraEditorGuids::SystemNamespaceMetaDataGuid);
					}

					if (!Usages.Contains(ENiagaraScriptUsage::EmitterSpawnScript) &&
						!Usages.Contains(ENiagaraScriptUsage::EmitterUpdateScript))
					{
						ExcludedNamespaceIds.Add(FNiagaraEditorGuids::EmitterNamespaceMetaDataGuid);
					}
				}
			}
		}

		// If culling parameter actions that match existing parameters in the graph, collect all IDs for parameters visited in the graph.
		if(bCullParameterActionsAlreadyInGraph)
		{ 
			const TMap<FNiagaraVariable, UNiagaraScriptVariable*>& VariableToScriptVariableMap = Graph->GetAllMetaData();
			for (auto It = VariableToScriptVariableMap.CreateConstIterator(); It; ++It)
			{
				ExistingGraphParameterIds.Add(It.Value()->Metadata.GetVariableGuid());
			}
		}
	}


	// Parameter collections
	if (NamespaceId == FNiagaraEditorGuids::ParameterCollectionNamespaceMetaDataGuid)
	{
		CollectParameterCollectionsActions(Collector);
	}
	// Engine intrinsic parameters
	else if (NamespaceId == FNiagaraEditorGuids::EngineNamespaceMetaDataGuid)
	{
		CollectEngineNamespaceParameterActions();
	}
	// Emitter intrinsic parameters
	else if (NamespaceId == FNiagaraEditorGuids::EmitterNamespaceMetaDataGuid)
	{
		CollectEmitterNamespaceParameterActions();
		CollectMakeNew(Collector, NamespaceId);
	}
	// DataInstance intrinsic parameters
	else if (NamespaceId == FNiagaraEditorGuids::DataInstanceNamespaceMetaDataGuid && (ExcludedNamespaceIds.Contains(FNiagaraEditorGuids::ParticleAttributeNamespaceMetaDataGuid) == false))
	{
		TArray<FNiagaraVariable> Variables;
		Variables.Add(SYS_PARAM_INSTANCE_ALIVE);
		AddParameterGroup(Collector, Variables, FNiagaraEditorGuids::DataInstanceNamespaceMetaDataGuid, FText(), 3, FString(), false);
	}
	// No NamespaceId set but still collecting engine namespace parameters (e.g. map get/set node menu.)
	else if (NamespaceId.IsValid() == false && bForceCollectEngineNamespaceParameterActions)
	{
		CollectEngineNamespaceParameterActions();
		CollectMakeNew(Collector, NamespaceId);
	}

	// Any other "unreserved" namespace
	else
	{
		CollectMakeNew(Collector, NamespaceId);
	}

	// Collect "add existing graph parameter" actions
	if (bShowGraphParameters)
	{
		for (const UNiagaraGraph* Graph : Graphs)
		{
			TMap<FNiagaraVariable, FNiagaraGraphParameterReferenceCollection> ParameterEntries = Graph->GetParameterReferenceMap();

			// Iterate the parameter reference map as this represents all parameters in the graph, including parameters the graph itself does not own.
			for (const auto& ParameterEntry : ParameterEntries)
			{
				const FNiagaraVariable& Parameter = ParameterEntry.Key;

				// Check if the graph owns the parameter (has a script variable for the parameter.)
				if (const UNiagaraScriptVariable* ScriptVar = Graph->GetScriptVariable(Parameter))
				{
					// The graph owns the parameter. Skip if it is a static switch.
					if (ScriptVar->GetIsStaticSwitch())
					{
						continue;
					}
					// Check that we do not add a duplicate entry.
					const FName& ParameterName = Parameter.GetName();
					if (VisitedParameterNames.Contains(ParameterName) == false)
					{
						// The script variable is not a duplicate, add an entry for it.
						VisitedParameterNames.Add(ParameterName);
						FNiagaraVariable MutableParameter = FNiagaraVariable(Parameter);
						const FText Category = bShowNamespaceCategory ? GetNamespaceCategoryText(NamespaceId) : LOCTEXT("NiagaraAddExistingParameterMenu", "Add Existing Parameter");
						const FText DisplayName = FText::FromName(Parameter.GetName());
						const FText Tooltip = FText::GetEmpty();
						TSharedPtr<FNiagaraMenuAction> Action(new FNiagaraMenuAction(
							Category, DisplayName, Tooltip, 0, FText::GetEmpty(),
							FNiagaraMenuAction::FOnExecuteStackAction::CreateSP(this, &SNiagaraAddParameterFromPanelMenu::ParameterSelected, MutableParameter)));
						Action->SetParameterVariable(Parameter);

						Collector.AddAction(Action, 3);
						continue;
					}
				}
			
				// The graph does not own the parameter, check if it is a reserved namespace parameter.
				const FNiagaraParameterHandle ParameterHandle = FNiagaraParameterHandle(Parameter.GetName());
				if (ParameterHandle.IsParameterCollectionHandle() || ParameterHandle.IsEngineHandle() || ParameterHandle.IsDataInstanceHandle())
				{
					// Check that we do not add a duplicate entry.
					const FName& ParameterName = Parameter.GetName();
					if (VisitedParameterNames.Contains(ParameterName) == false)
					{
						// The reserved namespace parameter is not a duplicate, add an entry for it.
						VisitedParameterNames.Add(ParameterName);
						FNiagaraVariable MutableParameter = FNiagaraVariable(Parameter);
						const FText Category = bShowNamespaceCategory ? GetNamespaceCategoryText(NamespaceId) : LOCTEXT("NiagaraAddExistingParameterMenu", "Add Existing Parameter");
						const FText DisplayName = FText::FromName(Parameter.GetName());
						const FText Tooltip = FText::GetEmpty();
						TSharedPtr<FNiagaraMenuAction> Action(new FNiagaraMenuAction(
							Category, DisplayName, Tooltip, 0, FText::GetEmpty(),
							FNiagaraMenuAction::FOnExecuteStackAction::CreateSP(this, &SNiagaraAddParameterFromPanelMenu::ParameterSelected, MutableParameter)));
						Action->SetParameterVariable(Parameter);

						Collector.AddAction(Action, 3);
						continue;
					}
				}
			}
		}
	}

	// Collect "add parameter from parameter definition asset" actions
	for (UNiagaraParameterDefinitions* ParameterDefinitions : AvailableParameterDefinitions)
	{
		bool bTopLevelCategory = ParameterDefinitions->GetIsPromotedToTopInAddMenus();
		const FText TopLevelCategory = FText::FromString(*ParameterDefinitions->GetName());
		const FText Category = bTopLevelCategory ? FText() : TopLevelCategory;
		for (const UNiagaraScriptVariable* ScriptVar : ParameterDefinitions->GetParametersConst())
		{
			// Only add parameters in the same namespace as the target namespace id if bOnlyShowParametersInNamespaceId is set.
			if (bOnlyShowParametersInNamespaceId && FNiagaraEditorUtilities::GetNamespaceMetaDataForVariableName(ScriptVar->Variable.GetName()).GetGuid() != NamespaceId)
			{
				continue;
			}

			// Check that we do not add a duplicate entry.
			const FGuid& ScriptVarId = ScriptVar->Metadata.GetVariableGuid();
			if (bCullParameterActionsAlreadyInGraph && ExistingGraphParameterIds.Contains(ScriptVarId))
			{
				continue;
			}
			else if (VisitedParameterNames.Contains(ScriptVar->Variable.GetName()))
			{
				continue;
			}

			const FText DisplayName = FText::FromName(ScriptVar->Variable.GetName());
			const FText& Tooltip = ScriptVar->Metadata.Description;
			TSharedPtr<FNiagaraMenuAction> Action(new FNiagaraMenuAction(
				Category, DisplayName, Tooltip, 0, FText::GetEmpty(),
				FNiagaraMenuAction::FOnExecuteStackAction::CreateSP(this, &SNiagaraAddParameterFromPanelMenu::ScriptVarFromParameterDefinitionsSelected, ScriptVar, ParameterDefinitions)));
			Action->SetParameterVariable(ScriptVar->Variable);
			
			if (bTopLevelCategory)
			{
				Collector.AddAction(Action, ParameterDefinitions->GetMenuSortOrder(), TopLevelCategory.ToString());
			}
			else
			{ 
				Action->SetSectionId(1); //Increment the default section id so parameter definitions actions are always categorized BELOW other actions.
				Collector.AddAction(Action, ParameterDefinitions->GetMenuSortOrder());
			}
		}
	}

	// Collect "add existing parameter" actions associated with Assignment nodes.
	if (AssignmentNode != nullptr)
	{
		// Gather required members for context from the AssignmentNode.
		UNiagaraNodeOutput* OutputNode = FNiagaraStackGraphUtilities::GetEmitterOutputNodeForStackNode(*AssignmentNode);
		UNiagaraSystem* OwningSystem = AssignmentNode->GetTypedOuter<UNiagaraSystem>();
		if (OwningSystem == nullptr)
		{
			return;
		}

		UNiagaraSystemEditorData* OwningSystemEditorData = Cast<UNiagaraSystemEditorData>(OwningSystem->GetEditorData());
		if (OwningSystemEditorData == nullptr)
		{
			return;
		}

		bool bOwningSystemIsPlaceholder = OwningSystemEditorData->GetOwningSystemIsPlaceholder();
		TOptional<FName> StackContextOverride = OutputNode->GetStackContextOverride();

		// Gather available parameters from the parameter map history.
		TArray<FNiagaraVariable> AvailableParameters;
		TArray<FName> CustomIterationNamespaces;
		TArray<FNiagaraParameterMapHistory> Histories = UNiagaraNodeParameterMapBase::GetParameterMaps(OutputNode->GetNiagaraGraph());
		for (FNiagaraParameterMapHistory& History : Histories)
		{
			for (int32 VarIdx = 0; VarIdx < History.Variables.Num(); VarIdx++)
			{
				FNiagaraVariable& Variable = History.Variables[VarIdx];
				if (StackContextOverride.IsSet() && StackContextOverride.GetValue() != NAME_None && Variable.IsInNameSpace(StackContextOverride.GetValue()))
				{
					AvailableParameters.AddUnique(Variable);
				}
				else if (History.IsPrimaryDataSetOutput(Variable, OutputNode->GetUsage()))
				{
					AvailableParameters.AddUnique(Variable);
				}
			}

			for (const FName& Namespace : History.IterationNamespaceOverridesEncountered)
			{
				CustomIterationNamespaces.AddUnique(Namespace);
			}
		}

		// Gather available parameters in the used namespace from the graph parameter to reference map and the system editor only parameters.
		TOptional<FName> UsageNamespace = FNiagaraStackGraphUtilities::GetNamespaceForOutputNode(OutputNode);
		if (UsageNamespace.IsSet())
		{
			for (const TPair<FNiagaraVariable, FNiagaraGraphParameterReferenceCollection>& Entry : OutputNode->GetNiagaraGraph()->GetParameterReferenceMap())
			{
				// Pick up any params with 0 references from the Parameters window
				bool bDoesParamHaveNoReferences = Entry.Value.ParameterReferences.Num() == 0;
				bool bIsParamInUsageNamespace = Entry.Key.IsInNameSpace(UsageNamespace.GetValue().ToString());

				if (bDoesParamHaveNoReferences && bIsParamInUsageNamespace)
				{
					AvailableParameters.AddUnique(Entry.Key);
				}
			}

			TSharedPtr<FNiagaraSystemViewModel> SystemViewModel = TNiagaraViewModelManager<UNiagaraSystem, FNiagaraSystemViewModel>::GetExistingViewModelForObject(OwningSystem);
			if (SystemViewModel.IsValid())
			{
				TArray<FNiagaraVariable> EditorOnlyParameters;
				for (const UNiagaraScriptVariable* EditorOnlyScriptVar : SystemViewModel->GetEditorOnlyParametersAdapter()->GetParameters())
				{
					const FNiagaraVariable& EditorOnlyParameter = EditorOnlyScriptVar->Variable;
					if (EditorOnlyParameter.IsInNameSpace(UsageNamespace.GetValue().ToString()))
					{
						AvailableParameters.AddUnique(EditorOnlyParameter);
					}
				}
			}
		}

		// Now check to see if any of the available write namespaces have overlap with the iteration namespaces. If so, we need to exclude them if they aren't the active stack context.
		// This is for situations like Emitter.Grid2DCollection.TestValue which should only be written if in the sim stage scripts and not emitter scripts, which would normally be allowed.
		TArray<FName> AvailableWriteNamespaces;
		FNiagaraStackGraphUtilities::GetNamespacesForNewWriteParameters(
			bOwningSystemIsPlaceholder ? FNiagaraStackGraphUtilities::EStackEditContext::Emitter : FNiagaraStackGraphUtilities::EStackEditContext::System,
			OutputNode->GetUsage(), StackContextOverride, AvailableWriteNamespaces);

		TArray<FName> ExclusionList;
		for (const FName& IterationNamespace : CustomIterationNamespaces)
		{
			FNiagaraVariableBase TempVar(FNiagaraTypeDefinition::GetFloatDef(), IterationNamespace);
			for (const FName& AvailableWriteNamespace : AvailableWriteNamespaces)
			{
				if (TempVar.IsInNameSpace(AvailableWriteNamespace))
				{
					if (!StackContextOverride.IsSet() || (StackContextOverride.IsSet() && IterationNamespace != StackContextOverride.GetValue()))
						ExclusionList.AddUnique(IterationNamespace);
				}
			}
		}

		// Cull available parameters if they are outside the available namespaces.
		for (const FNiagaraVariable& AvailableParameter : AvailableParameters)
		{
			bool bFound = false;
			// Now check to see if the variable is possible to write to
			for (const FName& AvailableWriteNamespace : AvailableWriteNamespaces)
			{
				if (AvailableParameter.IsInNameSpace(AvailableWriteNamespace))
				{
					bFound = true;
					break;
				}
			}

			if (!bFound)
			{
				continue;
			}
				
			// Now double-check that it doesn't overlap with a sub-namespace we're not allowed to write to
			bFound = false;
			for (const FName& ExcludedNamespace : ExclusionList)
			{
				if (AvailableParameter.IsInNameSpace(ExcludedNamespace))
				{
					bFound = true;
					break;
				}
			}

			if (bFound)
			{
				continue;
			}

			// Cull the available parameter if it has already been visited.
			const FName& ParameterName = AvailableParameter.GetName();
			if (VisitedParameterNames.Contains(ParameterName) == false)
			{
				// The parameter is not a duplicate, add an entry for it.
				VisitedParameterNames.Add(ParameterName);
				const FText Category = LOCTEXT("ModuleSetCategory", "Set Specific Parameters");
				const FText DisplayName = FText::FromName(AvailableParameter.GetName());
				const FText VarDesc = FNiagaraConstants::GetAttributeDescription(AvailableParameter);
				const FText Tooltip = FText::Format(LOCTEXT("SetFunctionPopupTooltip", "Description: Set the parameter {0}. {1}"), DisplayName, VarDesc);
				TSharedPtr<FNiagaraMenuAction> Action(new FNiagaraMenuAction(
					Category, DisplayName, Tooltip, 0, FText::GetEmpty(),
					FNiagaraMenuAction::FOnExecuteStackAction::CreateSP(this, &SNiagaraAddParameterFromPanelMenu::ParameterSelected, AvailableParameter)));
				Action->SetParameterVariable(AvailableParameter);
				Collector.AddAction(Action, 3);
			}
		}
	}

	Collector.AddAllActionsTo(OutAllActions);
}

void SNiagaraAddParameterFromPanelMenu::ParameterSelected(FNiagaraVariable NewVariable, const bool bCreateUniqueName /*= false*/, const FGuid InNamespaceId /*= FGuid()*/)
{
	auto GetNamespaceMetaData = [this, &InNamespaceId]()->const FNiagaraNamespaceMetadata {
		if (InNamespaceId.IsValid() == false)
		{
			if (bIsParameterReadNode) // Map Get
			{
				return FNiagaraEditorUtilities::GetNamespaceMetaDataForId(FNiagaraEditorGuids::ModuleNamespaceMetaDataGuid);
			}
			else // Map Set
			{
				return FNiagaraEditorUtilities::GetNamespaceMetaDataForId(FNiagaraEditorGuids::ModuleLocalNamespaceMetaDataGuid);
			}
		}
		return FNiagaraEditorUtilities::GetNamespaceMetaDataForId(InNamespaceId);
	};

	if (bCreateUniqueName)
	{
		FString TypeDisplayName;
		if (NewVariable.GetType().GetEnum() != nullptr)
		{
			TypeDisplayName = ((UField*)NewVariable.GetType().GetEnum())->GetDisplayNameText().ToString();
		}
		else if (NewVariable.GetType().GetStruct() != nullptr)
		{
			TypeDisplayName = NewVariable.GetType().GetStruct()->GetDisplayNameText().ToString();
		}
		else if (NewVariable.GetType().GetClass() != nullptr)
		{
			TypeDisplayName = NewVariable.GetType().GetClass()->GetDisplayNameText().ToString();
		}
		const FString NewVariableDefaultName = TypeDisplayName.IsEmpty()
			? TEXT("New Variable")
			: TEXT("New ") + TypeDisplayName;

		TArray<FString> NameParts;

		const FNiagaraNamespaceMetadata NamespaceMetaData = GetNamespaceMetaData();
		checkf(NamespaceMetaData.IsValid(), TEXT("Failed to get valid namespace metadata when creating unique name for parameter menu add parameter action!"));
		for (const FName Namespace : NamespaceMetaData.Namespaces)
		{
			NameParts.Add(Namespace.ToString());
		}

		if (NamespaceMetaData.RequiredNamespaceModifier != NAME_None)
		{
			NameParts.Add(NamespaceMetaData.RequiredNamespaceModifier.ToString());
		}

		NameParts.Add(NewVariableDefaultName);
		const FString ResultName = FString::Join(NameParts, TEXT("."));
		NewVariable.SetName(FName(*ResultName));
	}

	OnAddParameter.ExecuteIfBound(NewVariable);
}

void SNiagaraAddParameterFromPanelMenu::ParameterSelected(FNiagaraVariable NewVariable)
{
	ParameterSelected(NewVariable, false, FGuid());
}

void SNiagaraAddParameterFromPanelMenu::ScriptVarFromParameterDefinitionsSelected(const UNiagaraScriptVariable* NewScriptVar, UNiagaraParameterDefinitions* SourceParameterDefinitions)
{
	// If the parameter definitions the script var belongs to is not subscribed to, add it.
	const FGuid& SourceParameterDefinitionsId = SourceParameterDefinitions->GetDefinitionsUniqueId();
	if (SubscribedParameterDefinitions.ContainsByPredicate([SourceParameterDefinitionsId](const UNiagaraParameterDefinitions* ParameterDefinitions){ return ParameterDefinitions->GetDefinitionsUniqueId() == SourceParameterDefinitionsId; }) == false)
	{
		OnAddParameterDefinitions.ExecuteIfBound(SourceParameterDefinitions);
	}

	// Add the script var.
	OnAddScriptVar.ExecuteIfBound(NewScriptVar);
}

TSet<FName> SNiagaraAddParameterFromPanelMenu::GetAllGraphParameterNames() const
{
	TSet<FName> VisitedParameterNames;
	for (const UNiagaraGraph* Graph : Graphs)
	{
		const TMap<FNiagaraVariable, FNiagaraGraphParameterReferenceCollection> ParameterEntries = Graph->GetParameterReferenceMap();

		// Iterate the parameter reference map as this represents all parameters in the graph, including parameters the graph itself does not own.
		for (const auto& ParameterEntry : ParameterEntries)
		{
			VisitedParameterNames.Add(ParameterEntry.Key.GetName());
		}
	}
	return VisitedParameterNames;
}

FText SNiagaraAddParameterFromPanelMenu::GetSectionTitle(int32 SectionId)
{
	if (SectionId != 1)
	{
		ensureMsgf(false, TEXT("Encountered SectionId that was not \"1\"! Update formatting rules!"));
		return FText();
	}
	return LOCTEXT("ParameterDefinitionsSection", "Parameter Definitions");
}

///////////////////////////////////////////////////////////////////////////////
/// Add Parameter From Pin Menu												///
///////////////////////////////////////////////////////////////////////////////

void SNiagaraAddParameterFromPinMenu::Construct(const FArguments& InArgs)
{
	this->NiagaraNode = InArgs._NiagaraNode;
	this->AddPin = InArgs._AddPin;
	this->bIsParameterReadNode = AddPin->Direction == EEdGraphPinDirection::EGPD_Input ? false : true;

	SNiagaraParameterMenu::FArguments SuperArgs;
	SuperArgs._AutoExpandMenu = InArgs._AutoExpandMenu;
	SNiagaraParameterMenu::Construct(SuperArgs);
}


void SNiagaraAddParameterFromPinMenu::CollectAllActions(FGraphActionListBuilderBase& OutAllActions)
{
	FNiagaraMenuActionCollector Collector;
	TArray<FNiagaraTypeDefinition> Types(FNiagaraTypeRegistry::GetRegisteredTypes());
	Types.Sort([](const FNiagaraTypeDefinition& A, const FNiagaraTypeDefinition& B) { return (A.GetNameText().ToLower().ToString() < B.GetNameText().ToLower().ToString()); });

	for (const FNiagaraTypeDefinition& RegisteredType : Types)
	{
		bool bAllowType = false;
		bAllowType = NiagaraNode->AllowNiagaraTypeForAddPin(RegisteredType);

		if (bAllowType)
		{
			FNiagaraVariable Var(RegisteredType, FName(*RegisteredType.GetName()));
			FNiagaraEditorUtilities::ResetVariableToDefaultValue(Var);

			FText Category = FNiagaraEditorUtilities::GetVariableTypeCategory(Var);
			const FText DisplayName = RegisteredType.GetNameText();
			const FText Tooltip = FText::Format(LOCTEXT("AddButtonTypeEntryToolTipFormat", "Add a new {0} pin"), RegisteredType.GetNameText());
			const UEdGraphPin* ConstAddPin = AddPin;
			TSharedPtr<FNiagaraMenuAction> Action(new FNiagaraMenuAction(
				Category, DisplayName, Tooltip, 0, FText::GetEmpty(),
				FNiagaraMenuAction::FOnExecuteStackAction::CreateUObject(NiagaraNode, &UNiagaraNodeWithDynamicPins::AddParameter, Var, ConstAddPin)));

			Collector.AddAction(Action, 0);
		}
	}

	Collector.AddAllActionsTo(OutAllActions);
}

///////////////////////////////////////////////////////////////////////////////
/// Change Pin Type Menu													///
///////////////////////////////////////////////////////////////////////////////

void SNiagaraChangePinTypeMenu::Construct(const FArguments& InArgs)
{
	checkf(InArgs._PinToModify != nullptr, TEXT("Tried to construct change pin type menu without valid pin ptr!"));
	this->PinToModify = InArgs._PinToModify;

	SNiagaraParameterMenu::FArguments SuperArgs;
	SuperArgs._AutoExpandMenu = InArgs._AutoExpandMenu;
	SNiagaraParameterMenu::Construct(SuperArgs);
}

void SNiagaraChangePinTypeMenu::CollectAllActions(FGraphActionListBuilderBase& OutAllActions)
{
	FNiagaraMenuActionCollector Collector;
	UNiagaraNode* Node = Cast<UNiagaraNode>(PinToModify->GetOwningNode());
	checkf(Node, TEXT("Niagara node pin did not have a valid outer node!"));

	TArray<FNiagaraTypeDefinition> Types(FNiagaraTypeRegistry::GetRegisteredTypes());
	Types.Sort([](const FNiagaraTypeDefinition& A, const FNiagaraTypeDefinition& B) { return (A.GetNameText().ToLower().ToString() < B.GetNameText().ToLower().ToString()); });

	for (const FNiagaraTypeDefinition& RegisteredType : Types)
	{
		const bool bAllowType = Node->AllowNiagaraTypeForPinTypeChange(RegisteredType, PinToModify);

		if (bAllowType)
		{
			FNiagaraVariable Var(RegisteredType, FName(*RegisteredType.GetName()));
			FNiagaraEditorUtilities::ResetVariableToDefaultValue(Var);

			FText Category = FNiagaraEditorUtilities::GetVariableTypeCategory(Var);
			const FText DisplayName = RegisteredType.GetNameText();
			const FText Tooltip = FText::Format(LOCTEXT("ChangeSelectorTypeEntryToolTipFormat", "Change to {0} pin"), RegisteredType.GetNameText());
			TSharedPtr<FNiagaraMenuAction> Action(new FNiagaraMenuAction(
				Category, DisplayName, Tooltip, 0, FText::GetEmpty(),
				FNiagaraMenuAction::FOnExecuteStackAction::CreateUObject(Node, &UNiagaraNode::RequestNewPinType, PinToModify, RegisteredType)));

			Collector.AddAction(Action, 0);
		}
	}

	Collector.AddAllActionsTo(OutAllActions);
}

#undef LOCTEXT_NAMESPACE /*"SNiagaraParameterMenu"*/
