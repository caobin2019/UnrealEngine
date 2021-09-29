// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlRigBranchNodeSpawner.h"
#include "ControlRigUnitNodeSpawner.h"
#include "Graph/ControlRigGraph.h"
#include "Graph/ControlRigGraphNode.h"
#include "BlueprintNodeTemplateCache.h"
#include "RigVMModel/RigVMController.h"
#include "ControlRigBlueprint.h"
#include "ControlRigBlueprintUtils.h"
#include "RigVMModel/Nodes/RigVMBranchNode.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

#define LOCTEXT_NAMESPACE "ControlRigBranchNodeSpawner"

UControlRigBranchNodeSpawner* UControlRigBranchNodeSpawner::CreateGeneric(const FText& InMenuDesc, const FText& InCategory, const FText& InTooltip)
{
	UControlRigBranchNodeSpawner* NodeSpawner = NewObject<UControlRigBranchNodeSpawner>(GetTransientPackage());
	NodeSpawner->NodeClass = UControlRigGraphNode::StaticClass();

	FBlueprintActionUiSpec& MenuSignature = NodeSpawner->DefaultMenuSignature;
	
	MenuSignature.MenuName = InMenuDesc;
	MenuSignature.Tooltip  = InTooltip;
	MenuSignature.Category = InCategory;
	MenuSignature.Keywords = FText::FromString(TEXT("Switch,If,Branch,Condition,Else,Flip"));
	MenuSignature.Icon = FSlateIcon(TEXT("ControlRigEditorStyle"), TEXT("ControlRig.RigUnit"));

	return NodeSpawner;
}

FBlueprintNodeSignature UControlRigBranchNodeSpawner::GetSpawnerSignature() const
{
	return FBlueprintNodeSignature(NodeClass);
}

FBlueprintActionUiSpec UControlRigBranchNodeSpawner::GetUiSpec(FBlueprintActionContext const& Context, FBindingSet const& Bindings) const
{
	UEdGraph* TargetGraph = (Context.Graphs.Num() > 0) ? Context.Graphs[0] : nullptr;
	FBlueprintActionUiSpec MenuSignature = PrimeDefaultUiSpec(TargetGraph);

	DynamicUiSignatureGetter.ExecuteIfBound(Context, Bindings, &MenuSignature);
	return MenuSignature;
}

UEdGraphNode* UControlRigBranchNodeSpawner::Invoke(UEdGraph* ParentGraph, FBindingSet const& Bindings, FVector2D const Location) const
{
	UControlRigGraphNode* NewNode = nullptr;

	bool const bIsTemplateNode = FBlueprintNodeTemplateCache::IsTemplateOuter(ParentGraph);
	bool const bUndo = !bIsTemplateNode;

	// First create a backing member for our node
	UControlRigGraph* RigGraph = Cast<UControlRigGraph>(ParentGraph);
	check(RigGraph);
	UControlRigBlueprint* RigBlueprint = Cast<UControlRigBlueprint>(ParentGraph->GetOuter());
	check(RigBlueprint);

	FName MemberName = NAME_None;

#if WITH_EDITOR
	if (GEditor && !bIsTemplateNode)
	{
		GEditor->CancelTransaction(0);
	}
#endif

	URigVMController* Controller = bIsTemplateNode ? RigGraph->GetTemplateController() : RigBlueprint->Controller;

	FName Name = *URigVMBranchNode::BranchName;

	if (!bIsTemplateNode)
	{
		Controller->OpenUndoBracket(FString::Printf(TEXT("Add '%s' Node"), *Name.ToString()));
	}

	if (URigVMBranchNode* ModelNode = Controller->AddBranchNode(Location, Name.ToString(), bUndo))
	{
		NewNode = Cast<UControlRigGraphNode>(RigGraph->FindNodeForModelNodeName(ModelNode->GetFName()));

		if (NewNode && bUndo)
		{
			Controller->ClearNodeSelection(true);
			Controller->SelectNode(ModelNode, true, true);

			UControlRigUnitNodeSpawner::HookupMutableNode(ModelNode, RigBlueprint);
		}

		if (bUndo)
		{
			Controller->CloseUndoBracket();
		}
	}
	else
	{
		if (bUndo)
		{
			Controller->CancelUndoBracket();
		}
	}


	return NewNode;
}

bool UControlRigBranchNodeSpawner::IsTemplateNodeFilteredOut(FBlueprintActionFilter const& Filter) const
{
	return false;
}
#undef LOCTEXT_NAMESPACE
