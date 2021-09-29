// Copyright Epic Games, Inc. All Rights Reserved.

#include "SLiveLinkMessageBusSourceFactory.h"

#include "ILiveLinkClient.h"
#include "ILiveLinkModule.h"
#include "LiveLinkMessageBusDiscoveryManager.h"
#include "LiveLinkMessageBusFinder.h"
#include "Widgets/Layout/SBox.h"

#define LOCTEXT_NAMESPACE "LiveLinkMessageBusSourceEditor"

namespace ProviderPollUI
{
	const FName TypeColumnName(TEXT("Type"));
	const FName MachineColumnName(TEXT("Machine"));
};

namespace
{
	bool operator==(const FProviderPollResult& LHS, const FProviderPollResult& RHS)
	{
		return LHS.Name == RHS.Name && LHS.MachineName == RHS.MachineName;
	}
}

class SProviderPollRow : public SMultiColumnTableRow<FProviderPollResultPtr>
{
public:
	SLATE_BEGIN_ARGS(SProviderPollRow) {}

		/** The list item for this row */
		SLATE_ARGUMENT(FProviderPollResultPtr, PollResultPtr)

	SLATE_END_ARGS()

	void Construct(const FArguments& Args, const TSharedRef<STableViewBase>& OwnerTableView)
	{
		PollResultPtr = Args._PollResultPtr;

		SMultiColumnTableRow<FProviderPollResultPtr>::Construct(
			FSuperRowType::FArguments()
			.Padding(1.0f),
			OwnerTableView
		);
	}

	/** Overridden from SMultiColumnTableRow.  Generates a widget for this column of the list view. */
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
	{
		if (ColumnName == ProviderPollUI::TypeColumnName)
		{
			return	SNew(STextBlock)
					.Text(FText::FromString(PollResultPtr->Name));
		}
		else if (ColumnName == ProviderPollUI::MachineColumnName)
		{
			return	SNew(STextBlock)
				.Text(FText::FromString(PollResultPtr->MachineName));
		}

		return SNullWidget::NullWidget;
	}

private:
	FProviderPollResultPtr PollResultPtr;
};


SLiveLinkMessageBusSourceFactory::~SLiveLinkMessageBusSourceFactory()
{
	if (ILiveLinkModule* ModulePtr = FModuleManager::GetModulePtr<ILiveLinkModule>("LiveLink"))
	{
		ModulePtr->GetMessageBusDiscoveryManager().RemoveDiscoveryMessageRequest();
	}
}

void SLiveLinkMessageBusSourceFactory::Construct(const FArguments& Args)
{
	OnSourceSelected = Args._OnSourceSelected;

	ILiveLinkModule::Get().GetMessageBusDiscoveryManager().AddDiscoveryMessageRequest();

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.FillHeight(1)
		[
			SNew(SBox)
			.HeightOverride(200)
			.WidthOverride(200)
			[
				SAssignNew(ListView, SListView<TSharedPtr<FLiveLinkSource>>)
				.ListItemsSource(&Sources)
				.SelectionMode(ESelectionMode::SingleToggle)
				.OnGenerateRow(this, &SLiveLinkMessageBusSourceFactory::MakeSourceListViewWidget)
				.OnSelectionChanged(this, &SLiveLinkMessageBusSourceFactory::OnSourceListSelectionChanged)
				.HeaderRow
				(
					SNew(SHeaderRow)
					+ SHeaderRow::Column(ProviderPollUI::TypeColumnName)
					.FillWidth(43.0f)
					.DefaultLabel(LOCTEXT("TypeColumnHeaderName", "Source Type"))
					+ SHeaderRow::Column(ProviderPollUI::MachineColumnName)
					.FillWidth(43.0f)
					.DefaultLabel(LOCTEXT("MachineColumnHeaderName", "Source Machine"))
				)
			]
		]
	];
}

void SLiveLinkMessageBusSourceFactory::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	if (FApp::GetCurrentTime() - LastUIUpdateSeconds > 0.5)
	{
		PollData.Reset();
		PollData.Append(ILiveLinkModule::Get().GetMessageBusDiscoveryManager().GetDiscoveryResults());
		
		Sources.RemoveAllSwap([this](TSharedPtr<FLiveLinkSource> Source)
		{
			return FApp::GetCurrentTime() - Source->LastTimeSincePong > SecondsBeforeSourcesDisappear;
		});
		
		for (const FProviderPollResultPtr& PollResult : PollData)
		{
			TSharedPtr<FLiveLinkSource>* FoundSource = Sources.FindByPredicate([&PollResult](const TSharedPtr<FLiveLinkSource> Source){ return Source && PollResult && *Source->PollResult == *PollResult;});
			if (FoundSource && *FoundSource)
			{
				(*FoundSource)->LastTimeSincePong = FApp::GetCurrentTime();
			}
			else
			{
				Sources.Add(MakeShared<FLiveLinkSource>(PollResult));
			}
		}

		Sources.StableSort([](const TSharedPtr<FLiveLinkSource>& LHS, const TSharedPtr<FLiveLinkSource>& RHS)
		{
			if (LHS.IsValid() && RHS.IsValid() && LHS->PollResult.IsValid() && RHS->PollResult.IsValid())
			{
				return LHS->PollResult->Name.Compare(RHS->PollResult->Name) <= 0;
			}
			return true;
		});

		ListView->RequestListRefresh();
		LastUIUpdateSeconds = FApp::GetCurrentTime();
	}
}

TSharedRef<ITableRow> SLiveLinkMessageBusSourceFactory::MakeSourceListViewWidget(TSharedPtr<FLiveLinkSource> Source, const TSharedRef<STableViewBase>& OwnerTable) const
{
	return SNew(SProviderPollRow, OwnerTable).PollResultPtr(Source->PollResult);
}

void SLiveLinkMessageBusSourceFactory::OnSourceListSelectionChanged(TSharedPtr<FLiveLinkSource> Source, ESelectInfo::Type SelectionType)
{
	SelectedResult = Source->PollResult;
	OnSourceSelected.ExecuteIfBound(SelectedResult);
}

#undef LOCTEXT_NAMESPACE
