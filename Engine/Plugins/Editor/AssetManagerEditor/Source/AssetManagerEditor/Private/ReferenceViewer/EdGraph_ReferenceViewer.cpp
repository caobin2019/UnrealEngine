// Copyright Epic Games, Inc. All Rights Reserved.

#include "ReferenceViewer/EdGraph_ReferenceViewer.h"
#include "ReferenceViewer/EdGraphNode_Reference.h"
#include "EdGraph/EdGraphPin.h"
#include "ARFilter.h"
#include "AssetRegistryModule.h"
#include "AssetThumbnail.h"
#include "SReferenceViewer.h"
#include "SReferenceNode.h"
#include "GraphEditor.h"
#include "ICollectionManager.h"
#include "CollectionManagerModule.h"
#include "AssetManagerEditorModule.h"
#include "Engine/AssetManager.h"

UEdGraph_ReferenceViewer::UEdGraph_ReferenceViewer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AssetThumbnailPool = MakeShareable( new FAssetThumbnailPool(1024) );

	MaxSearchDepth = 1;
	MaxSearchBreadth = 20;

	bLimitSearchDepth = true;
	bLimitSearchBreadth = true;
	bIsShowSoftReferences = true;
	bIsShowHardReferences = true;
	bIsShowEditorOnlyReferences = true;
	bIsShowManagementReferences = false;
	bIsShowSearchableNames = false;
	bIsShowNativePackages = false;
	bIsShowReferencers = true;
	bIsShowDependencies = true;
	bIsShowFilteredPackagesOnly = false;
	bIsCompactMode = false;
}

void UEdGraph_ReferenceViewer::BeginDestroy()
{
	AssetThumbnailPool.Reset();

	Super::BeginDestroy();
}

void UEdGraph_ReferenceViewer::SetGraphRoot(const TArray<FAssetIdentifier>& GraphRootIdentifiers, const FIntPoint& GraphRootOrigin)
{
	CurrentGraphRootIdentifiers = GraphRootIdentifiers;
	CurrentGraphRootOrigin = GraphRootOrigin;

	// If we're focused on a searchable name, enable that flag
	for (const FAssetIdentifier& AssetId : GraphRootIdentifiers)
	{
		if (AssetId.IsValue())
		{
			bIsShowSearchableNames = true;
		}
		else if (AssetId.GetPrimaryAssetId().IsValid())
		{
			if (UAssetManager::IsValid())
			{
				UAssetManager::Get().UpdateManagementDatabase();
			}
			
			bIsShowManagementReferences = true;
		}
	}
}

const TArray<FAssetIdentifier>& UEdGraph_ReferenceViewer::GetCurrentGraphRootIdentifiers() const
{
	return CurrentGraphRootIdentifiers;
}

void UEdGraph_ReferenceViewer::SetReferenceViewer(TSharedPtr<SReferenceViewer> InViewer)
{
	ReferenceViewer = InViewer;
}

bool UEdGraph_ReferenceViewer::GetSelectedAssetsForMenuExtender(const class UEdGraphNode* Node, TArray<FAssetIdentifier>& SelectedAssets) const
{
	if (!ReferenceViewer.IsValid())
	{
		return false;
	}
	TSharedPtr<SGraphEditor> GraphEditor = ReferenceViewer.Pin()->GetGraphEditor();

	if (!GraphEditor.IsValid())
	{
		return false;
	}

	TSet<UObject*> SelectedNodes = GraphEditor->GetSelectedNodes();
	for (FGraphPanelSelectionSet::TConstIterator It(SelectedNodes); It; ++It)
	{
		if (UEdGraphNode_Reference* ReferenceNode = Cast<UEdGraphNode_Reference>(*It))
		{
			if (!ReferenceNode->IsCollapsed())
			{
				SelectedAssets.Add(ReferenceNode->GetIdentifier());
			}
		}
	}
	return true;
}

UEdGraphNode_Reference* UEdGraph_ReferenceViewer::RebuildGraph()
{
	RemoveAllNodes();
	UEdGraphNode_Reference* NewRootNode = ConstructNodes(CurrentGraphRootIdentifiers, CurrentGraphRootOrigin);
	NotifyGraphChanged();

	return NewRootNode;
}

bool UEdGraph_ReferenceViewer::IsSearchDepthLimited() const
{
	return bLimitSearchDepth;
}

bool UEdGraph_ReferenceViewer::IsSearchBreadthLimited() const
{
	return bLimitSearchBreadth;
}

bool UEdGraph_ReferenceViewer::IsShowSoftReferences() const
{
	return bIsShowSoftReferences;
}

bool UEdGraph_ReferenceViewer::IsShowHardReferences() const
{
	return bIsShowHardReferences;
}

bool UEdGraph_ReferenceViewer::IsShowFilteredPackagesOnly() const
{
	return bIsShowFilteredPackagesOnly;
}

bool UEdGraph_ReferenceViewer::IsCompactMode() const
{
	return bIsCompactMode;
}

bool UEdGraph_ReferenceViewer::IsShowEditorOnlyReferences() const
{
	return bIsShowEditorOnlyReferences;
}

bool UEdGraph_ReferenceViewer::IsShowManagementReferences() const
{
	return bIsShowManagementReferences;
}

bool UEdGraph_ReferenceViewer::IsShowSearchableNames() const
{
	return bIsShowSearchableNames;
}

bool UEdGraph_ReferenceViewer::IsShowNativePackages() const
{
	return bIsShowNativePackages;
}

bool UEdGraph_ReferenceViewer::IsShowReferencers() const
{
	return bIsShowReferencers;
}

bool UEdGraph_ReferenceViewer::IsShowDependencies() const
{
	return bIsShowDependencies;
}

void UEdGraph_ReferenceViewer::SetSearchDepthLimitEnabled(bool newEnabled)
{
	bLimitSearchDepth = newEnabled;
}

void UEdGraph_ReferenceViewer::SetSearchBreadthLimitEnabled(bool newEnabled)
{
	bLimitSearchBreadth = newEnabled;
}

void UEdGraph_ReferenceViewer::SetShowSoftReferencesEnabled(bool newEnabled)
{
	bIsShowSoftReferences = newEnabled;
}

void UEdGraph_ReferenceViewer::SetShowHardReferencesEnabled(bool newEnabled)
{
	bIsShowHardReferences = newEnabled;
}

void UEdGraph_ReferenceViewer::SetShowFilteredPackagesOnlyEnabled(bool newEnabled)
{
	bIsShowFilteredPackagesOnly = newEnabled;
}

void UEdGraph_ReferenceViewer::SetCompactModeEnabled(bool newEnabled)
{
	bIsCompactMode = newEnabled;
}

void UEdGraph_ReferenceViewer::SetShowEditorOnlyReferencesEnabled(bool newEnabled)
{
	bIsShowEditorOnlyReferences = newEnabled;
}

void UEdGraph_ReferenceViewer::SetShowManagementReferencesEnabled(bool newEnabled)
{
	bIsShowManagementReferences = newEnabled;
}

void UEdGraph_ReferenceViewer::SetShowSearchableNames(bool newEnabled)
{
	bIsShowSearchableNames = newEnabled;
}

void UEdGraph_ReferenceViewer::SetShowNativePackages(bool newEnabled)
{
	bIsShowNativePackages = newEnabled;
}

void UEdGraph_ReferenceViewer::SetShowReferencers(const bool newEnabled)
{
	bIsShowReferencers = newEnabled;
}

void UEdGraph_ReferenceViewer::SetShowDependencies(const bool newEnabled)
{
	bIsShowDependencies = newEnabled;
}

int32 UEdGraph_ReferenceViewer::GetSearchDepthLimit() const
{
	return MaxSearchDepth;
}

int32 UEdGraph_ReferenceViewer::GetSearchBreadthLimit() const
{
	return MaxSearchBreadth;
}

void UEdGraph_ReferenceViewer::SetSearchDepthLimit(int32 NewDepthLimit)
{
	MaxSearchDepth = NewDepthLimit;
}

void UEdGraph_ReferenceViewer::SetSearchBreadthLimit(int32 NewBreadthLimit)
{
	MaxSearchBreadth = NewBreadthLimit;
}

FName UEdGraph_ReferenceViewer::GetCurrentCollectionFilter() const
{
	return CurrentCollectionFilter;
}

void UEdGraph_ReferenceViewer::SetCurrentCollectionFilter(FName NewFilter)
{
	CurrentCollectionFilter = NewFilter;
}

bool UEdGraph_ReferenceViewer::GetEnableCollectionFilter() const
{
	return bEnableCollectionFilter;
}

void UEdGraph_ReferenceViewer::SetEnableCollectionFilter(bool bEnabled)
{
	bEnableCollectionFilter = bEnabled;
}

FAssetManagerDependencyQuery UEdGraph_ReferenceViewer::GetReferenceSearchFlags(bool bHardOnly) const
{
	using namespace UE::AssetRegistry;
	FAssetManagerDependencyQuery Query;
	Query.Categories = EDependencyCategory::None;
	Query.Flags = EDependencyQuery::NoRequirements;

	bool bLocalIsShowSoftReferences = bIsShowSoftReferences && !bHardOnly;
	if (bLocalIsShowSoftReferences || bIsShowHardReferences)
	{
		Query.Categories |= EDependencyCategory::Package;
		Query.Flags |= bLocalIsShowSoftReferences ? EDependencyQuery::NoRequirements : EDependencyQuery::Hard;
		Query.Flags |= bIsShowHardReferences ? EDependencyQuery::NoRequirements : EDependencyQuery::Soft;
		Query.Flags |= bIsShowEditorOnlyReferences ? EDependencyQuery::NoRequirements : EDependencyQuery::Game;
	}
	if (bIsShowSearchableNames && !bHardOnly)
	{
		Query.Categories |= EDependencyCategory::SearchableName;
	}
	if (bIsShowManagementReferences)
	{
		Query.Categories |= EDependencyCategory::Manage;
		Query.Flags |= bHardOnly ? EDependencyQuery::Direct : EDependencyQuery::NoRequirements;
	}

	return Query;
}

UEdGraphNode_Reference* UEdGraph_ReferenceViewer::ConstructNodes(const TArray<FAssetIdentifier>& GraphRootIdentifiers, const FIntPoint& GraphRootOrigin )
{
	UEdGraphNode_Reference* RootNode = NULL;

	if (GraphRootIdentifiers.Num() > 0)
	{
		// It both were false, nothing (other than the GraphRootIdentifiers) would be displayed
		check(bIsShowReferencers || bIsShowDependencies);

		TSet<FName> AllowedPackageNames;
		if (ShouldFilterByCollection())
		{
			FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();
			TArray<FName> AssetPaths;
			CollectionManagerModule.Get().GetAssetsInCollection(CurrentCollectionFilter, ECollectionShareType::CST_All, AssetPaths);
			AllowedPackageNames.Reserve(AssetPaths.Num());
			for (FName AssetPath : AssetPaths)
			{
				AllowedPackageNames.Add(FName(*FPackageName::ObjectPathToPackageName(AssetPath.ToString())));
			}
		}

		TMap<FAssetIdentifier, int32> ReferencerNodeSizes;
		TSet<FAssetIdentifier> VisitedReferencerSizeNames;
		if (bIsShowReferencers)
		{
			const int32 ReferencerDepth = 1;
			RecursivelyGatherSizes(/*bReferencers=*/true, GraphRootIdentifiers, AllowedPackageNames, ReferencerDepth, VisitedReferencerSizeNames, ReferencerNodeSizes);
		}

		TMap<FAssetIdentifier, int32> DependencyNodeSizes;
		TSet<FAssetIdentifier> VisitedDependencySizeNames;
		if (bIsShowDependencies)
		{
			const int32 DependencyDepth = 1;
			RecursivelyGatherSizes(/*bReferencers=*/false, GraphRootIdentifiers, AllowedPackageNames, DependencyDepth, VisitedDependencySizeNames, DependencyNodeSizes);
		}

		TSet<FName> AllPackageNames;

		auto AddPackage = [](const FAssetIdentifier& AssetId, TSet<FName>& PackageNames)
		{ 
			// Only look for asset data if this is a package
			if (!AssetId.IsValue() && !AssetId.PackageName.IsNone())
			{
				PackageNames.Add(AssetId.PackageName);
			}
		};

		if (bIsShowReferencers)
		{
			for (const FAssetIdentifier& AssetId : VisitedReferencerSizeNames)
			{
				AddPackage(AssetId, AllPackageNames);
			}
		}

		if (bIsShowDependencies)
		{
			for (const FAssetIdentifier& AssetId : VisitedDependencySizeNames)
			{
				AddPackage(AssetId, AllPackageNames);
			}
		}

		TMap<FName, FAssetData> PackagesToAssetDataMap;
		GatherAssetData(AllPackageNames, PackagesToAssetDataMap);

		// Create the root node
		RootNode = CreateReferenceNode();
		RootNode->SetupReferenceNode(GraphRootOrigin, GraphRootIdentifiers, PackagesToAssetDataMap.FindRef(GraphRootIdentifiers[0].PackageName), /*bInAllowThumbnail = */ !bIsCompactMode);

		if (bIsShowReferencers)
		{
			TSet<FAssetIdentifier> VisitedReferencerNames;
			const int32 VisitedReferencerDepth = 1;
			RecursivelyConstructNodes(/*bReferencers=*/true, RootNode, GraphRootIdentifiers, GraphRootOrigin, ReferencerNodeSizes, PackagesToAssetDataMap, AllowedPackageNames, VisitedReferencerDepth, VisitedReferencerNames);
		}

		if (bIsShowDependencies)
		{
			TSet<FAssetIdentifier> VisitedDependencyNames;
			const int32 VisitedDependencyDepth = 1;
			RecursivelyConstructNodes(/*bReferencers=*/false, RootNode, GraphRootIdentifiers, GraphRootOrigin, DependencyNodeSizes, PackagesToAssetDataMap, AllowedPackageNames, VisitedDependencyDepth, VisitedDependencyNames);
		}
	}

	return RootNode;
}

void UEdGraph_ReferenceViewer::GetSortedLinks(const TArray<FAssetIdentifier>& Identifiers, bool bReferencers, const FAssetManagerDependencyQuery& Query, TMap<FAssetIdentifier, EDependencyPinCategory>& OutLinks) const
{
	using namespace UE::AssetRegistry;
	auto CategoryOrder = [](EDependencyCategory InCategory)
	{
		switch (InCategory)
		{
		case EDependencyCategory::Package: return 0;
		case EDependencyCategory::Manage: return 1;
		case EDependencyCategory::SearchableName: return 2;
		default: check(false);  return 3;
		}
	};
	auto IsHard = [](EDependencyProperty Properties)
	{
		return static_cast<bool>(((Properties & EDependencyProperty::Hard) != EDependencyProperty::None) | ((Properties & EDependencyProperty::Direct) != EDependencyProperty::None));
	};

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	TArray<FAssetDependency> LinksToAsset;
	for (const FAssetIdentifier& AssetId : Identifiers)
	{
		LinksToAsset.Reset();
		if (bReferencers)
		{
			AssetRegistry.GetReferencers(AssetId, LinksToAsset, Query.Categories, Query.Flags);
		}
		else
		{
			AssetRegistry.GetDependencies(AssetId, LinksToAsset, Query.Categories, Query.Flags);
		}

		// Sort the links from most important kind of link to least important kind of link, so that if we can't display them all in an ExceedsMaxSearchBreadth test, we
		// show the most important links.
		Algo::Sort(LinksToAsset, [&CategoryOrder, &IsHard](const FAssetDependency& A, const FAssetDependency& B)
			{
				if (A.Category != B.Category)
				{
					return CategoryOrder(A.Category) < CategoryOrder(B.Category);
				}
				if (A.Properties != B.Properties)
				{
					bool bAIsHard = IsHard(A.Properties);
					bool bBIsHard = IsHard(B.Properties);
					if (bAIsHard != bBIsHard)
					{
						return bAIsHard;
					}
				}
				return A.AssetId.PackageName.LexicalLess(B.AssetId.PackageName);
			});
		for (FAssetDependency LinkToAsset : LinksToAsset)
		{
			EDependencyPinCategory& Category = OutLinks.FindOrAdd(LinkToAsset.AssetId, EDependencyPinCategory::LinkEndActive);
			bool bIsHard = IsHard(LinkToAsset.Properties);
			bool bIsUsedInGame = (LinkToAsset.Category != EDependencyCategory::Package) | ((LinkToAsset.Properties & EDependencyProperty::Game) != EDependencyProperty::None);
			Category |= EDependencyPinCategory::LinkEndActive;
			Category |= bIsHard ? EDependencyPinCategory::LinkTypeHard : EDependencyPinCategory::LinkTypeNone;
			Category |= bIsUsedInGame ? EDependencyPinCategory::LinkTypeUsedInGame : EDependencyPinCategory::LinkTypeNone;
		}
	}

	for (TMap<FAssetIdentifier, EDependencyPinCategory>::TIterator It(OutLinks); It; ++It)
	{
		if (!IsPackageIdentifierPassingFilter(It.Key()))
		{
			It.RemoveCurrent();
		}
	}
}

bool UEdGraph_ReferenceViewer::IsPackageIdentifierPassingFilter(const FAssetIdentifier& InAssetIdentifier) const
{
	if (!InAssetIdentifier.IsValue())
	{
		if (!bIsShowNativePackages && InAssetIdentifier.PackageName.ToString().StartsWith(TEXT("/Script")))
		{
			return false;
		}

		if (bIsShowFilteredPackagesOnly && IsPackageNamePassingFilterCallback.IsSet() && !(*IsPackageNamePassingFilterCallback)(InAssetIdentifier.PackageName))
		{
			return false;
		}
	}

	return true;
}

int32 UEdGraph_ReferenceViewer::RecursivelyGatherSizes(bool bReferencers, const TArray<FAssetIdentifier>& Identifiers, const TSet<FName>& AllowedPackageNames, int32 CurrentDepth, TSet<FAssetIdentifier>& VisitedNames, TMap<FAssetIdentifier, int32>& OutNodeSizes) const
{
	check(Identifiers.Num() > 0);

	VisitedNames.Append(Identifiers);

	TMap<FAssetIdentifier, EDependencyPinCategory> ReferenceLinks;
	GetSortedLinks(Identifiers, bReferencers, GetReferenceSearchFlags(false), ReferenceLinks);

	TArray<FAssetIdentifier> ReferenceNames;
	ReferenceNames.Reserve(ReferenceLinks.Num());
	for (const TPair<FAssetIdentifier, EDependencyPinCategory>& Pair : ReferenceLinks)
	{
		ReferenceNames.Add(Pair.Key);
	}

	int32 NodeSize = 0;
	if ( ReferenceNames.Num() > 0 && !ExceedsMaxSearchDepth(CurrentDepth) )
	{
		int32 NumReferencesMade = 0;
		int32 NumReferencesExceedingMax = 0;

		// Filter for our registry source
		IAssetManagerEditorModule::Get().FilterAssetIdentifiersForCurrentRegistrySource(ReferenceNames, GetReferenceSearchFlags(false), !bReferencers);

		// Since there are referencers, use the size of all your combined referencers.
		// Do not count your own size since there could just be a horizontal line of nodes
		for (FAssetIdentifier& AssetId : ReferenceNames)
		{
			if ( !VisitedNames.Contains(AssetId) && (!AssetId.IsPackage() || !ShouldFilterByCollection() || AllowedPackageNames.Contains(AssetId.PackageName)) )
			{
				if ( !ExceedsMaxSearchBreadth(NumReferencesMade) )
				{
					TArray<FAssetIdentifier> NewPackageNames;
					NewPackageNames.Add(AssetId);
					NodeSize += RecursivelyGatherSizes(bReferencers, NewPackageNames, AllowedPackageNames, CurrentDepth + 1, VisitedNames, OutNodeSizes);
					NumReferencesMade++;
				}
				else
				{
					NumReferencesExceedingMax++;
				}
			}
		}

		if ( NumReferencesExceedingMax > 0 )
		{
			// Add one size for the collapsed node
			NodeSize++;
		}
	}

	if ( NodeSize == 0 )
	{
		// If you have no valid children, the node size is just 1 (counting only self to make a straight line)
		NodeSize = 1;
	}

	OutNodeSizes.Add(Identifiers[0], NodeSize);
	return NodeSize;
}

void UEdGraph_ReferenceViewer::GatherAssetData(const TSet<FName>& AllPackageNames, TMap<FName, FAssetData>& OutPackageToAssetDataMap) const
{
	// Take a guess to find the asset instead of searching for it. Most packages have a single asset in them with the same name as the package.
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	FARFilter Filter;
	for ( auto PackageIt = AllPackageNames.CreateConstIterator(); PackageIt; ++PackageIt )
	{
		const FString& PackageName = (*PackageIt).ToString();
		Filter.PackageNames.Add(*PackageIt);
	}

	TArray<FAssetData> AssetDataList;
	AssetRegistryModule.Get().GetAssets(Filter, AssetDataList);
	for ( auto AssetIt = AssetDataList.CreateConstIterator(); AssetIt; ++AssetIt )
	{
		OutPackageToAssetDataMap.Add((*AssetIt).PackageName, *AssetIt);
	}
}

UEdGraphNode_Reference* UEdGraph_ReferenceViewer::RecursivelyConstructNodes(bool bReferencers, UEdGraphNode_Reference* RootNode, const TArray<FAssetIdentifier>& Identifiers, const FIntPoint& NodeLoc, const TMap<FAssetIdentifier, int32>& NodeSizes, const TMap<FName, FAssetData>& PackagesToAssetDataMap, const TSet<FName>& AllowedPackageNames, int32 CurrentDepth, TSet<FAssetIdentifier>& VisitedNames)
{
	check(Identifiers.Num() > 0);

	VisitedNames.Append(Identifiers);

	UEdGraphNode_Reference* NewNode = NULL;
	if ( RootNode->GetIdentifier() == Identifiers[0] )
	{
		// Don't create the root node. It is already created!
		NewNode = RootNode;
	}
	else
	{
		NewNode = CreateReferenceNode();
		NewNode->SetupReferenceNode(NodeLoc, Identifiers, PackagesToAssetDataMap.FindRef(Identifiers[0].PackageName), /*bInAllowThumbnail = */ !bIsCompactMode);
	}

	TMap<FAssetIdentifier, EDependencyPinCategory> Referencers;
	GetSortedLinks(Identifiers, bReferencers, GetReferenceSearchFlags(false), Referencers);

	if (Referencers.Num() > 0 && !ExceedsMaxSearchDepth(CurrentDepth))
	{
		FIntPoint ReferenceNodeLoc = NodeLoc;

		if (bReferencers)
		{
			// Referencers go left
			ReferenceNodeLoc.X -= bIsCompactMode ? 400 : 800;
		}
		else
		{
			// Dependencies go right
			ReferenceNodeLoc.X += bIsCompactMode ? 400 : 800;
		}

		const int32 NodeSizeY = bIsCompactMode ? 100 : 200;
		const int32 TotalReferenceSizeY = NodeSizes.FindChecked(Identifiers[0]) * NodeSizeY;

		ReferenceNodeLoc.Y -= TotalReferenceSizeY * 0.5f;
		ReferenceNodeLoc.Y += NodeSizeY * 0.5f;

		int32 NumReferencesMade = 0;
		int32 NumReferencesExceedingMax = 0;

		// Filter for our registry source
		TArray<FAssetIdentifier> ReferenceIds;
		Referencers.GenerateKeyArray(ReferenceIds);
		IAssetManagerEditorModule::Get().FilterAssetIdentifiersForCurrentRegistrySource(ReferenceIds, GetReferenceSearchFlags(false), !bReferencers);
		if (ReferenceIds.Num() != Referencers.Num())
		{
			TSet<FAssetIdentifier> KeptSet;
			KeptSet.Reserve(ReferenceIds.Num());
			for (FAssetIdentifier Id : ReferenceIds)
			{
				KeptSet.Add(Id);
			}
			for (TMap<FAssetIdentifier, EDependencyPinCategory>::TIterator It(Referencers); It; ++It)
			{
				if (!KeptSet.Contains(It.Key()))
				{
					It.RemoveCurrent();
				}
			}
		}
		for ( TPair<FAssetIdentifier, EDependencyPinCategory>& DependencyPair : Referencers)
		{
			FAssetIdentifier ReferenceName = DependencyPair.Key;

			if ( !VisitedNames.Contains(ReferenceName) && (!ReferenceName.IsPackage() || !ShouldFilterByCollection() || AllowedPackageNames.Contains(ReferenceName.PackageName)) )
			{
				if ( !ExceedsMaxSearchBreadth(NumReferencesMade) )
				{
					int32 ThisNodeSizeY = ReferenceName.IsValue() ? 100 : NodeSizeY;

					const int32 RefSizeY = NodeSizes.FindChecked(ReferenceName);
					FIntPoint RefNodeLoc;
					RefNodeLoc.X = ReferenceNodeLoc.X;
					RefNodeLoc.Y = ReferenceNodeLoc.Y + RefSizeY * ThisNodeSizeY * 0.5 - ThisNodeSizeY * 0.5;
					
					TArray<FAssetIdentifier> NewIdentifiers;
					NewIdentifiers.Add(ReferenceName);
					
					UEdGraphNode_Reference* ReferenceNode = RecursivelyConstructNodes(bReferencers, RootNode, NewIdentifiers, RefNodeLoc, NodeSizes, PackagesToAssetDataMap, AllowedPackageNames, CurrentDepth + 1, VisitedNames);
					if ( ensure(ReferenceNode) )
					{
						if (bReferencers)
						{
							ReferenceNode->GetDependencyPin()->PinType.PinCategory = ::GetName(DependencyPair.Value);
						}
						else
						{
							ReferenceNode->GetReferencerPin()->PinType.PinCategory = ::GetName(DependencyPair.Value);
						}

						if ( bReferencers )
						{
							NewNode->AddReferencer( ReferenceNode );
						}
						else
						{
							ReferenceNode->AddReferencer( NewNode );
						}

						ReferenceNodeLoc.Y += RefSizeY * ThisNodeSizeY;
					}

					NumReferencesMade++;
				}
				else
				{
					NumReferencesExceedingMax++;
				}
			}
		}

		if ( NumReferencesExceedingMax > 0 )
		{
			// There are more references than allowed to be displayed. Make a collapsed node.
			UEdGraphNode_Reference* ReferenceNode = CreateReferenceNode();
			FIntPoint RefNodeLoc;
			RefNodeLoc.X = ReferenceNodeLoc.X;
			RefNodeLoc.Y = ReferenceNodeLoc.Y;

			if ( ensure(ReferenceNode) )
			{
				ReferenceNode->SetAllowThumbnail(!bIsCompactMode);
				ReferenceNode->SetReferenceNodeCollapsed(RefNodeLoc, NumReferencesExceedingMax);

				if ( bReferencers )
				{
					NewNode->AddReferencer( ReferenceNode );
				}
				else
				{
					ReferenceNode->AddReferencer( NewNode );
				}
			}
		}
	}

	return NewNode;
}

const TSharedPtr<FAssetThumbnailPool>& UEdGraph_ReferenceViewer::GetAssetThumbnailPool() const
{
	return AssetThumbnailPool;
}

bool UEdGraph_ReferenceViewer::ExceedsMaxSearchDepth(int32 Depth) const
{
	// ExceedsMaxSearchDepth requires only greater (not equal than) because, even though the Depth is 1-based indexed (similarly to Breadth), the first index (index 0) corresponds to the root object 
	return bLimitSearchDepth && Depth > MaxSearchDepth;
}

bool UEdGraph_ReferenceViewer::ExceedsMaxSearchBreadth(int32 Breadth) const
{
	// ExceedsMaxSearchBreadth requires greater or equal than because the Breadth is 1-based indexed
	return bLimitSearchBreadth && Breadth >= MaxSearchBreadth;
}

UEdGraphNode_Reference* UEdGraph_ReferenceViewer::CreateReferenceNode()
{
	const bool bSelectNewNode = false;
	return Cast<UEdGraphNode_Reference>(CreateNode(UEdGraphNode_Reference::StaticClass(), bSelectNewNode));
}

void UEdGraph_ReferenceViewer::RemoveAllNodes()
{
	TArray<UEdGraphNode*> NodesToRemove = Nodes;
	for (int32 NodeIndex = 0; NodeIndex < NodesToRemove.Num(); ++NodeIndex)
	{
		RemoveNode(NodesToRemove[NodeIndex]);
	}
}

bool UEdGraph_ReferenceViewer::ShouldFilterByCollection() const
{
	return bEnableCollectionFilter && CurrentCollectionFilter != NAME_None;
}
